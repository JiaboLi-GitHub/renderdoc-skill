// Compile PipeState method implementations from renderdoc's pipestate.inl
// These methods are not exported from renderdoc.dll, so we compile them directly.
#include "renderdoc_replay.h"

// pipestate.inl uses ToStr<unsigned int> which calls DoStringise<unsigned int>,
// not exported from renderdoc.dll. Provide a local implementation.
#include <cstdio>
template <>
rdcstr DoStringise(const unsigned int &val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", val);
    return rdcstr(buf);
}

#include "pipestate.inl"
