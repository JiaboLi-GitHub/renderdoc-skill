#pragma once

class Command
{
public:
    virtual ~Command() = default;
    virtual const char *name() = 0;
    virtual const char *description() = 0;
    // argc/argv are the arguments AFTER the subcommand name
    virtual int execute(int argc, char **argv) = 0;
};
