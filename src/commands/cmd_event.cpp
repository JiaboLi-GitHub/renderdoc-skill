#include <cstdlib>
#include <cstring>
#include <functional>
#include "command.h"
#include "replay_context.h"
#include "json_output.h"

// ResourceId wraps a private uint64_t. No public getter exists outside RENDERDOC_EXPORTS.
// Use memcpy to extract the numeric value for JSON serialization.
// This is safe because ResourceId is a trivial struct wrapping a single uint64_t.
static uint64_t resourceIdToUint64(ResourceId id)
{
    uint64_t val = 0;
    memcpy(&val, &id, sizeof(val));
    return val;
}

static bool isNullResourceId(ResourceId id)
{
    return id == ResourceId();
}

class EventCommand : public Command
{
public:
    const char *name() override { return "event"; }
    const char *description() override { return "Show pipeline state for a specific event"; }

    int execute(int argc, char **argv) override
    {
        if(argc < 1)
            return json_output::error("Usage: renderdoc-tool event <rdc-file> --eid <eventId>");

        std::string path = argv[0];
        uint32_t eid = 0;
        bool eidFound = false;

        for(int i = 1; i < argc; i++)
        {
            if(strcmp(argv[i], "--eid") == 0 && i + 1 < argc)
            {
                eid = (uint32_t)atoi(argv[i + 1]);
                eidFound = true;
                i++;
            }
        }

        if(!eidFound)
            return json_output::error("Missing --eid parameter");

        ReplayContext ctx;
        auto err = ctx.openWithReplay(path);
        if(err)
            return json_output::error(*err);

        IReplayController *r = ctx.controller();

        // Navigate to the event
        r->SetFrameEvent(eid, true);

        const PipeState &pipe = r->GetPipelineState();
        const SDFile &sdFile = r->GetStructuredFile();

        // Find action name for this event
        std::string eventName;
        std::function<bool(const rdcarray<ActionDescription> &)> findAction;
        findAction = [&](const rdcarray<ActionDescription> &actions) -> bool {
            for(const auto &a : actions)
            {
                if(a.eventId == eid)
                {
                    eventName = std::string(a.GetName(sdFile).c_str());
                    return true;
                }
                if(findAction(a.children))
                    return true;
            }
            return false;
        };
        findAction(r->GetRootActions());

        nlohmann::json j;
        j["eventId"] = eid;
        j["name"] = eventName;

        // Vertex shader
        {
            ResourceId vsId = pipe.GetShader(ShaderStage::Vertex);
            nlohmann::json vs;
            if(!isNullResourceId(vsId))
            {
                vs["resourceId"] = resourceIdToUint64(vsId);
                vs["entryPoint"] = std::string(pipe.GetShaderEntryPoint(ShaderStage::Vertex).c_str());
            }
            else
            {
                vs = nullptr;
            }
            j["vertexShader"] = vs;
        }

        // Pixel/Fragment shader
        {
            ResourceId psId = pipe.GetShader(ShaderStage::Fragment);
            nlohmann::json ps;
            if(!isNullResourceId(psId))
            {
                ps["resourceId"] = resourceIdToUint64(psId);
                ps["entryPoint"] = std::string(pipe.GetShaderEntryPoint(ShaderStage::Fragment).c_str());
            }
            else
            {
                ps = nullptr;
            }
            j["pixelShader"] = ps;
        }

        // Viewport
        {
            Viewport vp = pipe.GetViewport(0);
            nlohmann::json vpj;
            vpj["x"] = vp.x;
            vpj["y"] = vp.y;
            vpj["width"] = vp.width;
            vpj["height"] = vp.height;
            j["viewport"] = vpj;
        }

        // Render targets
        {
            rdcarray<Descriptor> rts = pipe.GetOutputTargets();
            nlohmann::json rtArr = nlohmann::json::array();
            for(const auto &rt : rts)
            {
                if(rt.resource == ResourceId())
                    continue;
                nlohmann::json rtj;
                rtj["resourceId"] = resourceIdToUint64(rt.resource);
                rtj["format"] = std::string(rt.format.Name().c_str());
                rtArr.push_back(rtj);
            }
            j["renderTargets"] = rtArr;
        }

        // Depth target
        {
            Descriptor dt = pipe.GetDepthTarget();
            if(dt.resource != ResourceId())
            {
                nlohmann::json dtj;
                dtj["resourceId"] = resourceIdToUint64(dt.resource);
                dtj["format"] = std::string(dt.format.Name().c_str());
                j["depthTarget"] = dtj;
            }
            else
            {
                j["depthTarget"] = nullptr;
            }
        }

        return json_output::success(j);
    }
};

std::unique_ptr<Command> makeEventCommand()
{
    return std::make_unique<EventCommand>();
}
