# renderdoc-tool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++ CLI tool that wraps RenderDoc's replay API, exposing `open`, `info`, `draws`, and `event` subcommands with JSON output for AI agent consumption.

**Architecture:** Single executable (`renderdoc-tool.exe`) with subcommand dispatch. Each command independently opens an `.rdc` file via `ICaptureFile`/`IReplayController`, queries data, and outputs JSON to stdout. Three layers: `ReplayContext` (lifecycle), `Command` (business logic), `json_output` (serialization).

**Tech Stack:** C++17, CMake, RenderDoc replay API (`renderdoc.dll`), nlohmann/json (header-only)

**Spec:** `docs/superpowers/specs/2026-03-25-renderdoc-tool-design.md`

---

## File Map

| File | Responsibility |
|------|---------------|
| `CMakeLists.txt` | Build config: find RenderDoc headers/lib, compile, link |
| `src/main.cpp` | Entry point, `REPLAY_PROGRAM_MARKER()`, init/shutdown, subcommand dispatch |
| `src/include/command.h` | `Command` base class (pure virtual interface) |
| `src/include/replay_context.h` | `ReplayContext` class declaration |
| `src/include/json_output.h` | `json_output::success()` / `json_output::error()` declarations |
| `src/core/replay_context.cpp` | `ReplayContext::open()`, `openWithReplay()`, destructor cleanup |
| `src/core/json_output.cpp` | JSON stdout output implementation |
| `src/commands/cmd_open.cpp` | `OpenCommand` — validate .rdc, output metadata |
| `src/commands/cmd_info.cpp` | `InfoCommand` — frame summary statistics |
| `src/commands/cmd_draws.cpp` | `DrawsCommand` — flattened action list |
| `src/commands/cmd_event.cpp` | `EventCommand` — pipeline state for single event |
| `third_party/nlohmann/json.hpp` | Header-only JSON library (vendored) |

---

### Task 1: CMake + Project Skeleton

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp` (minimal stub)

- [ ] **Step 1: Create CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(renderdoc-tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RenderDoc paths
set(RENDERDOC_DIR "D:/renderdoc" CACHE PATH "Path to RenderDoc source")
set(RENDERDOC_LIB_DIR "${RENDERDOC_DIR}/x64/Release" CACHE PATH "Path to renderdoc.lib/dll")

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(renderdoc-tool ${SOURCES})

target_include_directories(renderdoc-tool PRIVATE
    src/include
    third_party
    ${RENDERDOC_DIR}/renderdoc/api/replay
    ${RENDERDOC_DIR}/renderdoc/api  # for sibling includes like app/
)

target_link_libraries(renderdoc-tool PRIVATE
    ${RENDERDOC_LIB_DIR}/renderdoc.lib
)

# Copy renderdoc.dll to output directory
add_custom_command(TARGET renderdoc-tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RENDERDOC_LIB_DIR}/renderdoc.dll"
        $<TARGET_FILE_DIR:renderdoc-tool>
)
```

- [ ] **Step 2: Create minimal main.cpp**

```cpp
#include <cstdio>
#include "renderdoc_replay.h"

REPLAY_PROGRAM_MARKER()

int main(int argc, char **argv)
{
    printf("renderdoc-tool v0.1\n");
    return 0;
}
```

- [ ] **Step 3: Build and verify it compiles and links**

```bash
cd D:/renderdoc-skill
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected: Compiles successfully, `build/Release/renderdoc-tool.exe` exists.

- [ ] **Step 4: Run it to verify DLL loads**

```bash
./build/Release/renderdoc-tool.exe
```

Expected: Prints `renderdoc-tool v0.1` and exits 0.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/main.cpp
git commit -m "feat: project skeleton with CMake build and RenderDoc linking"
```

---

### Task 2: nlohmann/json + json_output Utilities

**Files:**
- Create: `third_party/nlohmann/json.hpp` (download)
- Create: `src/include/json_output.h`
- Create: `src/core/json_output.cpp`

- [ ] **Step 1: Download nlohmann/json single header**

```bash
mkdir -p third_party/nlohmann
curl -L -o third_party/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
```

- [ ] **Step 2: Create json_output.h**

```cpp
#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace json_output {

// Print JSON to stdout and return exit code 0
int success(const nlohmann::json& data);

// Print error JSON to stdout and return exit code 1
int error(const std::string& message);

}  // namespace json_output
```

- [ ] **Step 3: Create json_output.cpp**

```cpp
#include "json_output.h"
#include <cstdio>

namespace json_output {

int success(const nlohmann::json& data)
{
    std::string out = data.dump(2);
    printf("%s\n", out.c_str());
    return 0;
}

int error(const std::string& message)
{
    nlohmann::json j;
    j["status"] = "error";
    j["message"] = message;
    std::string out = j.dump(2);
    printf("%s\n", out.c_str());
    return 1;
}

}  // namespace json_output
```

- [ ] **Step 4: Verify build still compiles**

```bash
cmake --build build --config Release
```

Expected: Compiles with no errors.

- [ ] **Step 5: Commit**

```bash
git add third_party/nlohmann/json.hpp src/include/json_output.h src/core/json_output.cpp
git commit -m "feat: add json_output utilities with nlohmann/json"
```

---

### Task 3: Command Base Class + Dispatch in main.cpp

**Files:**
- Create: `src/include/command.h`
- Modify: `src/main.cpp`

- [ ] **Step 1: Create command.h**

```cpp
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
```

- [ ] **Step 2: Rewrite main.cpp with dispatch logic**

```cpp
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "renderdoc_replay.h"
#include "command.h"
#include "json_output.h"

REPLAY_PROGRAM_MARKER()

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
    // commands["open"] = std::make_unique<OpenCommand>();

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
```

- [ ] **Step 3: Build and verify**

```bash
cmake --build build --config Release
./build/Release/renderdoc-tool.exe
```

Expected: Prints usage with "Commands:" header (empty list for now), exits 1.

- [ ] **Step 4: Commit**

```bash
git add src/include/command.h src/main.cpp
git commit -m "feat: add Command base class and subcommand dispatch"
```

---

### Task 4: ReplayContext

**Files:**
- Create: `src/include/replay_context.h`
- Create: `src/core/replay_context.cpp`

- [ ] **Step 1: Create replay_context.h**

```cpp
#pragma once

#include <optional>
#include <string>
#include "renderdoc_replay.h"

class ReplayContext
{
public:
    ReplayContext() = default;
    ~ReplayContext();

    // Non-copyable
    ReplayContext(const ReplayContext &) = delete;
    ReplayContext &operator=(const ReplayContext &) = delete;

    // Open .rdc file without creating replay controller (lightweight).
    // Returns error message on failure, std::nullopt on success.
    std::optional<std::string> open(const std::string &path);

    // Open .rdc file AND create replay controller (heavyweight).
    // Required for info/draws/event commands.
    // Returns error message on failure, std::nullopt on success.
    std::optional<std::string> openWithReplay(const std::string &path);

    ICaptureFile *captureFile() { return m_captureFile; }
    IReplayController *controller() { return m_controller; }

private:
    ICaptureFile *m_captureFile = nullptr;
    IReplayController *m_controller = nullptr;
};
```

- [ ] **Step 2: Create replay_context.cpp**

```cpp
#include "replay_context.h"

ReplayContext::~ReplayContext()
{
    if(m_controller)
    {
        m_controller->Shutdown();
        m_controller = nullptr;
    }
    if(m_captureFile)
    {
        m_captureFile->Shutdown();
        m_captureFile = nullptr;
    }
}

std::optional<std::string> ReplayContext::open(const std::string &path)
{
    m_captureFile = RENDERDOC_OpenCaptureFile();
    if(!m_captureFile)
        return "Failed to create capture file handle";

    ResultDetails result = m_captureFile->OpenFile(rdcstr(path.c_str()), rdcstr(), nullptr);
    if(!result.OK())
    {
        std::string msg = "Failed to open file: ";
        msg += std::string(result.Message().c_str());
        m_captureFile->Shutdown();
        m_captureFile = nullptr;
        return msg;
    }

    return std::nullopt;
}

std::optional<std::string> ReplayContext::openWithReplay(const std::string &path)
{
    auto err = open(path);
    if(err)
        return err;

    ReplayOptions opts;
    auto openResult = m_captureFile->OpenCapture(opts, nullptr);
    if(!openResult.first.OK())
    {
        std::string msg = "Failed to create replay controller: ";
        msg += std::string(openResult.first.Message().c_str());
        return msg;
    }

    m_controller = openResult.second;
    return std::nullopt;
}
```

Note: `ICaptureFile::OpenCapture(const ReplayOptions &opts, RENDERDOC_ProgressCallback progress)` at `renderdoc_replay.h:1740` returns `rdcpair<ResultDetails, IReplayController *>`. `rdcpair` does NOT support C++17 structured bindings (it has user-declared constructors making it non-aggregate, and no `tuple_size`/`get<>` specializations). Use `.first`/`.second` member access instead.

- [ ] **Step 3: Build and verify**

```bash
cmake --build build --config Release
```

Expected: Compiles with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/include/replay_context.h src/core/replay_context.cpp
git commit -m "feat: add ReplayContext for capture file lifecycle management"
```

---

### Task 5: `open` Command

**Files:**
- Create: `src/commands/cmd_open.cpp`
- Modify: `src/main.cpp` (register command)

- [ ] **Step 1: Create cmd_open.cpp**

```cpp
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
```

Note: `DriverName()` is inherited from `ICaptureAccess` (which `ICaptureFile` extends at `renderdoc_replay.h:1578`). The return value is an `rdcstr` — the exact format may vary (e.g., `"D3D11"` vs `"Direct3D 11"`). Test with a real `.rdc` to see actual output and adjust if needed.

- [ ] **Step 2: Register in main.cpp**

Add at the top of main.cpp:

```cpp
// Forward declarations of command factories
std::unique_ptr<Command> makeOpenCommand();
```

In cmd_open.cpp, add at the bottom:

```cpp
std::unique_ptr<Command> makeOpenCommand()
{
    return std::make_unique<OpenCommand>();
}
```

In main.cpp, register it:

```cpp
    commands["open"] = makeOpenCommand();
```

This pattern avoids including command headers in main.cpp while keeping each command self-contained.

- [ ] **Step 3: Build and test with a real .rdc file**

```bash
cmake --build build --config Release
./build/Release/renderdoc-tool.exe open <path-to-test.rdc>
```

Expected: JSON output with `status`, `machineIdent`, `replaySupport`.

- [ ] **Step 4: Test with invalid file**

```bash
./build/Release/renderdoc-tool.exe open nonexistent.rdc
```

Expected: `{"status": "error", "message": "Failed to open file: ..."}`, exit code 1.

- [ ] **Step 5: Commit**

```bash
git add src/commands/cmd_open.cpp src/main.cpp
git commit -m "feat: add open command for .rdc file validation"
```

---

### Task 6: `info` Command

**Files:**
- Create: `src/commands/cmd_info.cpp`
- Modify: `src/main.cpp` (register command)

- [ ] **Step 1: Create cmd_info.cpp**

This command needs a full replay controller to query frame info, textures, buffers, and action count.

```cpp
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
```

Note: `IReplayController::GetAPIProperties()` at `renderdoc_replay.h` returns `APIProperties` which contains `GraphicsAPI pipelineType`. The `GraphicsAPI` enum is in `replay_enums.h`.

- [ ] **Step 2: Register in main.cpp**

Add declaration and registration:

```cpp
std::unique_ptr<Command> makeInfoCommand();
// ...
commands["info"] = makeInfoCommand();
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build --config Release
./build/Release/renderdoc-tool.exe info <path-to-test.rdc>
```

Expected: JSON with filename, api, frameNumber, totalActions, totalTextures, totalBuffers.

- [ ] **Step 4: Commit**

```bash
git add src/commands/cmd_info.cpp src/main.cpp
git commit -m "feat: add info command for frame summary"
```

---

### Task 7: `draws` Command

**Files:**
- Create: `src/commands/cmd_draws.cpp`
- Modify: `src/main.cpp` (register command)

- [ ] **Step 1: Create cmd_draws.cpp**

```cpp
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
```

- [ ] **Step 2: Register in main.cpp**

```cpp
std::unique_ptr<Command> makeDrawsCommand();
// ...
commands["draws"] = makeDrawsCommand();
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build --config Release
./build/Release/renderdoc-tool.exe draws <path-to-test.rdc>
```

Expected: JSON array of actions, each with eventId, actionId, name, flags.

- [ ] **Step 4: Commit**

```bash
git add src/commands/cmd_draws.cpp src/main.cpp
git commit -m "feat: add draws command for action listing"
```

---

### Task 8: `event` Command

**Files:**
- Create: `src/commands/cmd_event.cpp`
- Modify: `src/main.cpp` (register command)

- [ ] **Step 1: Create cmd_event.cpp**

```cpp
#include <cstdlib>
#include <cstring>
#include <cstring>  // memcpy
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
```

**ResourceId serialization note:** `ResourceId` (in `resourceid.h`) wraps a private `uint64_t id` with no public getter outside `RENDERDOC_EXPORTS`. The `resourceIdToUint64()` helper uses `memcpy` to extract the raw value. This is safe because `ResourceId` is a trivially-copyable struct containing only a single `uint64_t`. There is no `IsValid()` method — use `id == ResourceId()` to check for null.

- [ ] **Step 2: Register in main.cpp**

```cpp
std::unique_ptr<Command> makeEventCommand();
// ...
commands["event"] = makeEventCommand();
```

- [ ] **Step 3: Build and test with a draw call event**

```bash
cmake --build build --config Release
# First find a valid eventId from draws output
./build/Release/renderdoc-tool.exe draws <test.rdc> | head -5
# Then query that event
./build/Release/renderdoc-tool.exe event <test.rdc> --eid <eventId>
```

Expected: JSON with eventId, name, vertexShader, pixelShader, viewport, renderTargets, depthTarget.

- [ ] **Step 4: Test with non-draw event**

```bash
./build/Release/renderdoc-tool.exe event <test.rdc> --eid 1
```

Expected: JSON output with null/empty shader fields (graceful degradation).

- [ ] **Step 5: Commit**

```bash
git add src/commands/cmd_event.cpp src/main.cpp
git commit -m "feat: add event command for pipeline state inspection"
```

---

### Task 9: End-to-End Verification

- [ ] **Step 1: Full command test sequence**

```bash
RDC=<path-to-test.rdc>
./build/Release/renderdoc-tool.exe open $RDC
./build/Release/renderdoc-tool.exe info $RDC
./build/Release/renderdoc-tool.exe draws $RDC
# Pick an eventId from draws output for a Drawcall-flagged action
./build/Release/renderdoc-tool.exe event $RDC --eid <drawcall-eid>
```

Verify all four commands produce valid JSON and exit 0.

- [ ] **Step 2: Error case tests**

```bash
./build/Release/renderdoc-tool.exe open nonexistent.rdc     # file not found
./build/Release/renderdoc-tool.exe info                      # missing arg
./build/Release/renderdoc-tool.exe event test.rdc            # missing --eid
./build/Release/renderdoc-tool.exe unknown test.rdc          # unknown command
```

Verify all produce error JSON or usage message, and exit non-zero.

- [ ] **Step 3: Final commit if any fixes were needed**

```bash
git add -A
git commit -m "fix: end-to-end verification fixes"
```
