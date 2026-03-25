#include "command.h"
#include "replay_context.h"
#include "json_output.h"

static nlohmann::json flagsToJson(ActionFlags flags)
{
    nlohmann::json arr = nlohmann::json::array();

    uint32_t f = (uint32_t)flags;

    if(f & (uint32_t)ActionFlags::Clear)        arr.push_back("Clear");
    if(f & (uint32_t)ActionFlags::Drawcall)     arr.push_back("Drawcall");
    if(f & (uint32_t)ActionFlags::Dispatch)     arr.push_back("Dispatch");
    if(f & (uint32_t)ActionFlags::MeshDispatch) arr.push_back("MeshDispatch");
    if(f & (uint32_t)ActionFlags::CmdList)      arr.push_back("CmdList");
    if(f & (uint32_t)ActionFlags::SetMarker)    arr.push_back("SetMarker");
    if(f & (uint32_t)ActionFlags::PushMarker)   arr.push_back("PushMarker");
    if(f & (uint32_t)ActionFlags::PopMarker)    arr.push_back("PopMarker");
    if(f & (uint32_t)ActionFlags::Present)      arr.push_back("Present");
    if(f & (uint32_t)ActionFlags::MultiAction)  arr.push_back("MultiAction");
    if(f & (uint32_t)ActionFlags::Copy)         arr.push_back("Copy");
    if(f & (uint32_t)ActionFlags::Resolve)      arr.push_back("Resolve");
    if(f & (uint32_t)ActionFlags::GenMips)      arr.push_back("GenMips");
    if(f & (uint32_t)ActionFlags::PassBoundary) arr.push_back("PassBoundary");
    if(f & (uint32_t)ActionFlags::Indexed)      arr.push_back("Indexed");
    if(f & (uint32_t)ActionFlags::Instanced)    arr.push_back("Instanced");
    if(f & (uint32_t)ActionFlags::Auto)         arr.push_back("Auto");
    if(f & (uint32_t)ActionFlags::Indirect)     arr.push_back("Indirect");
    if(f & (uint32_t)ActionFlags::ClearColor)   arr.push_back("ClearColor");
    if(f & (uint32_t)ActionFlags::ClearDepthStencil) arr.push_back("ClearDepthStencil");
    if(f & (uint32_t)ActionFlags::BeginPass)    arr.push_back("BeginPass");
    if(f & (uint32_t)ActionFlags::EndPass)      arr.push_back("EndPass");

    return arr;
}

static void flattenActions(const rdcarray<ActionDescription> &actions,
                           const SDFile &sdFile,
                           nlohmann::json &out)
{
    for(const auto &a : actions)
    {
        nlohmann::json item;
        item["eventId"] = a.eventId;
        item["actionId"] = a.actionId;
        item["name"] = std::string(a.GetName(sdFile).c_str());
        item["flags"] = flagsToJson(a.flags);
        out.push_back(item);

        flattenActions(a.children, sdFile, out);
    }
}

class DrawsCommand : public Command
{
public:
    const char *name() override { return "draws"; }
    const char *description() override { return "List all draw calls and actions"; }

    int execute(int argc, char **argv) override
    {
        if(argc < 1)
            return json_output::error("Usage: renderdoc-tool draws <rdc-file>");

        std::string path = argv[0];

        ReplayContext ctx;
        auto err = ctx.openWithReplay(path);
        if(err)
            return json_output::error(*err);

        IReplayController *r = ctx.controller();
        const auto &actions = r->GetRootActions();
        const SDFile &sdFile = r->GetStructuredFile();

        nlohmann::json arr = nlohmann::json::array();
        flattenActions(actions, sdFile, arr);

        return json_output::success(arr);
    }
};

std::unique_ptr<Command> makeDrawsCommand()
{
    return std::make_unique<DrawsCommand>();
}
