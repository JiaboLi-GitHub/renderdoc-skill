#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "renderdoc_replay.h"
#include "command.h"
#include "json_output.h"

REPLAY_PROGRAM_MARKER()

std::unique_ptr<Command> makeOpenCommand();
std::unique_ptr<Command> makeInfoCommand();
std::unique_ptr<Command> makeDrawsCommand();

static std::map<std::string, std::unique_ptr<Command>> commands;

static void print_usage()
{
    fprintf(stderr, "Usage: renderdoc-tool <command> <rdc-file> [options]\n\n");
    fprintf(stderr, "Commands:\n");
    for(auto &kv : commands)
        fprintf(stderr, "  %-10s %s\n", kv.first.c_str(), kv.second->description());
}

int main(int argc, char **argv)
{
    GlobalEnvironment env;
    rdcarray<rdcstr> args;
    RENDERDOC_InitialiseReplay(env, args);

    // Register commands here as they are implemented
    commands["open"] = makeOpenCommand();
    commands["info"] = makeInfoCommand();
    commands["draws"] = makeDrawsCommand();

    if(argc < 2)
    {
        print_usage();
        RENDERDOC_ShutdownReplay();
        return 1;
    }

    std::string cmdName = argv[1];

    auto it = commands.find(cmdName);
    if(it == commands.end())
    {
        fprintf(stderr, "Unknown command: %s\n\n", cmdName.c_str());
        print_usage();
        RENDERDOC_ShutdownReplay();
        return 1;
    }

    int ret = it->second->execute(argc - 2, argv + 2);

    RENDERDOC_ShutdownReplay();
    return ret;
}
