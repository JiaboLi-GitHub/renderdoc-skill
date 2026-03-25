#include "command.h"
#include "replay_context.h"
#include "json_output.h"

class OpenCommand : public Command
{
public:
    const char *name() override { return "open"; }
    const char *description() override { return "Validate .rdc file and show metadata"; }

    int execute(int argc, char **argv) override
    {
        if(argc < 1)
            return json_output::error("Usage: renderdoc-tool open <rdc-file>");

        std::string path = argv[0];

        ReplayContext ctx;
        auto err = ctx.open(path);
        if(err)
            return json_output::error(*err);

        ICaptureFile *cap = ctx.captureFile();

        nlohmann::json j;
        j["status"] = "ok";
        j["driver"] = std::string(cap->DriverName().c_str());
        j["machineIdent"] = std::string(cap->RecordedMachineIdent().c_str());

        ReplaySupport support = cap->LocalReplaySupport();
        j["replaySupport"] = (support == ReplaySupport::Supported);

        return json_output::success(j);
    }
};

std::unique_ptr<Command> makeOpenCommand()
{
    return std::make_unique<OpenCommand>();
}
