#include "command.h"
#include "replay_context.h"
#include "json_output.h"

static int countActions(const rdcarray<ActionDescription> &actions)
{
    int count = 0;
    for(const auto &a : actions)
    {
        count++;
        count += countActions(a.children);
    }
    return count;
}

class InfoCommand : public Command
{
public:
    const char *name() override { return "info"; }
    const char *description() override { return "Show frame summary information"; }

    int execute(int argc, char **argv) override
    {
        if(argc < 1)
            return json_output::error("Usage: renderdoc-tool info <rdc-file>");

        std::string path = argv[0];

        ReplayContext ctx;
        auto err = ctx.openWithReplay(path);
        if(err)
            return json_output::error(*err);

        IReplayController *r = ctx.controller();

        FrameDescription frame = r->GetFrameInfo();
        const auto &actions = r->GetRootActions();
        const auto &textures = r->GetTextures();
        const auto &buffers = r->GetBuffers();

        APIProperties props = r->GetAPIProperties();

        nlohmann::json j;
        j["filename"] = path;
        j["api"] = std::string(props.pipelineType == GraphicsAPI::D3D11   ? "D3D11"
                               : props.pipelineType == GraphicsAPI::D3D12 ? "D3D12"
                               : props.pipelineType == GraphicsAPI::OpenGL ? "OpenGL"
                               : props.pipelineType == GraphicsAPI::Vulkan ? "Vulkan"
                                                                           : "Unknown");
        j["frameNumber"] = frame.frameNumber;
        j["totalActions"] = countActions(actions);
        j["totalTextures"] = (int)textures.size();
        j["totalBuffers"] = (int)buffers.size();

        return json_output::success(j);
    }
};

std::unique_ptr<Command> makeInfoCommand()
{
    return std::make_unique<InfoCommand>();
}
