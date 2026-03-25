# renderdoc-tool Design Spec

## Overview

A standalone C++ CLI tool (`renderdoc-tool.exe`) that wraps RenderDoc's C++ replay API, enabling Claude Code AI agents to open and analyze `.rdc` GPU capture files interactively via Bash commands with JSON output.

## Goals

- **MVP scope**: 4 commands — `open`, `info`, `draws`, `event`
- **AI agent friendly**: Structured JSON output on stdout, error JSON on failure
- **Stateless**: Each invocation independently loads the .rdc file (no daemon/session)
- **Minimal dependencies**: Only RenderDoc DLL + nlohmann/json (header-only)

## Non-Goals (for MVP)

- Texture/buffer export
- Pixel debugging
- Frame diffing
- Daemon/session mode
- Shader source/disassembly
- Remote replay

## Architecture

### Project Structure

```
D:\renderdoc-skill\
├── CMakeLists.txt              # Top-level build config
├── src/
│   ├── main.cpp                # Entry point, subcommand dispatch
│   ├── commands/
│   │   ├── cmd_open.cpp        # open — validate .rdc file
│   │   ├── cmd_info.cpp        # info — frame summary
│   │   ├── cmd_draws.cpp       # draws — draw call list
│   │   └── cmd_event.cpp       # event — single event pipeline state
│   ├── core/
│   │   ├── replay_context.cpp  # ICaptureFile + IReplayController lifecycle
│   │   └── json_output.cpp     # JSON serialization utilities
│   └── include/
│       ├── command.h           # Command base class
│       ├── replay_context.h
│       └── json_output.h
└── third_party/
    └── nlohmann/json.hpp       # Header-only JSON library
```

### Build System

- **CMake** project
- `RENDERDOC_DIR` variable points to `D:\renderdoc`
- Includes: `${RENDERDOC_DIR}/renderdoc/api/replay/` headers
- Links: pre-built import lib at `${RENDERDOC_DIR}/x64/Release/renderdoc.lib`
- Output: `renderdoc-tool.exe` (requires `renderdoc.dll` at runtime, copy from `${RENDERDOC_DIR}/x64/Release/`)
- Must use `REPLAY_PROGRAM_MARKER()` macro in main.cpp to prevent RenderDoc self-capture

### Implementation Notes

- RenderDoc uses its own container types (`rdcarray<T>`, `rdcstr`) instead of STL. All API calls use these types, not `std::vector`/`std::string`.
- `GlobalEnvironment` struct must be default-constructed for `RENDERDOC_InitialiseReplay()`.
- `ActionDescription::GetName()` requires a `const SDFile&` parameter obtained from `IReplayController::GetStructuredFile()`.
- `PipeState` (API-agnostic) should be used instead of API-specific state objects (e.g., `GetD3D11PipelineState()`), so the tool works across D3D11, D3D12, Vulkan, and OpenGL captures.
- `ResourceFormat` has a `Name()` method for string serialization of texture formats.
- Stateless architecture means each invocation replays the capture's resource creation. Expect 5-30 seconds per command on large captures.

### Core Components

#### ReplayContext

Encapsulates the full RenderDoc replay lifecycle. All commands share this:

```cpp
class ReplayContext {
public:
    // Open .rdc and create replay controller. Returns error message on failure.
    std::optional<std::string> open(const std::string& path);

    ICaptureFile*      captureFile();
    IReplayController* controller();

    ~ReplayContext();  // Automatically closes controller and captureFile
};
```

Every command follows the same flow:
1. Parse arguments
2. `ReplayContext::open(path)`
3. Query data via `controller()`
4. Serialize to JSON, output to stdout

#### Command Base Class

```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual const char* name() = 0;
    virtual const char* description() = 0;
    virtual int execute(int argc, char** argv) = 0;
};
```

#### main.cpp Dispatch

```cpp
int main(int argc, char** argv) {
    RENDERDOC_InitialiseReplay(env, args);

    // Register commands
    commands["open"]  = std::make_unique<OpenCommand>();
    commands["info"]  = std::make_unique<InfoCommand>();
    commands["draws"] = std::make_unique<DrawsCommand>();
    commands["event"] = std::make_unique<EventCommand>();

    // Match subcommand and execute
    auto it = commands.find(argv[1]);
    int ret = it->second->execute(argc - 2, argv + 2);

    RENDERDOC_ShutdownReplay();
    return ret;
}
```

#### JSON Output Utilities

Thin wrapper over nlohmann/json:

```cpp
namespace json_output {
    void success(const nlohmann::json& data);  // stdout JSON, exit 0
    void error(const std::string& msg);        // stdout error JSON, exit 1
}
```

## Command Specifications

### Invocation Convention

```
renderdoc-tool <command> <rdc-file> [options]
```

- Success: stdout outputs JSON, exit code 0
- Failure: stdout outputs `{"status": "error", "message": "..."}`, exit code 1

### `open` — Validate .rdc file

```bash
renderdoc-tool open test.rdc
```

Output:
```json
{"status": "ok", "driver": "D3D11", "machineIdent": "windows64", "replaySupport": true}
```

Opens file and validates metadata without creating a full replay controller. This is a lightweight check — no GPU device creation. `machineIdent` is a string (from `RecordedMachineIdent()`), `replaySupport` indicates whether the local machine can replay this capture.

RenderDoc API calls: `RENDERDOC_OpenCaptureFile()` → `ICaptureFile::OpenFile()` → `ICaptureFile::LocalReplaySupport()` → close.

### `info` — Frame summary

```bash
renderdoc-tool info test.rdc
```

Output:
```json
{
  "filename": "test.rdc",
  "driver": "D3D11",
  "frameNumber": 0,
  "totalActions": 1523,
  "totalTextures": 47,
  "totalBuffers": 82
}
```

RenderDoc API calls: `GetFrameInfo()`, `GetRootActions()` (count), `GetTextures()`, `GetBuffers()`.

### `draws` — Draw call list

```bash
renderdoc-tool draws test.rdc
```

Output:
```json
[
  {"eventId": 1, "actionId": 0, "name": "ClearRenderTargetView", "flags": ["Clear"]},
  {"eventId": 15, "actionId": 1, "name": "DrawIndexed(36)", "flags": ["Drawcall", "Indexed"]}
]
```

Recursively traverses `GetRootActions()` and flattens all actions into a list. Action names are obtained via `ActionDescription::GetName(controller->GetStructuredFile())`.

### `event` — Single event pipeline state

```bash
renderdoc-tool event test.rdc --eid 15
```

Output:
```json
{
  "eventId": 15,
  "name": "DrawIndexed(36)",
  "vertexShader": {"resourceId": 42, "entryPoint": "VSMain"},
  "pixelShader": {"resourceId": 43, "entryPoint": "PSMain"},
  "viewport": {"x": 0, "y": 0, "width": 1920, "height": 1080},
  "renderTargets": [{"resourceId": 5, "format": "R8G8B8A8_UNORM"}],
  "depthTarget": {"resourceId": 6, "format": "D24_UNORM_S8_UINT"}
}
```

RenderDoc API calls: `SetFrameEvent(eid, true)` → `GetPipelineState()` (returns API-agnostic `PipeState`) → extract VS/PS via `GetShader()`/`GetShaderEntryPoint()`, viewport via `GetViewport()`, render targets via `GetOutputTargets()`, depth target via `GetDepthTarget()`.

Note: Some events (e.g., resource creation, map/unmap) have no meaningful pipeline state. In this case, shader fields will be null and the output will reflect the partial state available.

## Error Handling

All errors are reported as JSON on stdout with exit code 1:

```json
{"status": "error", "message": "Failed to open file: file not found"}
```

Error categories:
- File not found or unreadable
- Invalid/corrupt .rdc file
- Replay controller creation failure (e.g., GPU driver mismatch)
- Invalid event ID (for `event` command)
- Event has no pipeline state (non-draw event) — returns partial JSON, not an error
- Missing required arguments

## Future Extensions (post-MVP)

- `shader` — shader source/disassembly
- `texture` — texture export as PNG
- `buffer` — buffer data export
- `pipeline` — full pipeline state dump
- `pixel` — pixel history debugging
- `diff` — frame comparison
- Daemon mode for session persistence
- `--format tsv` output mode
