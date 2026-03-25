# renderdoc-tool

A stateless C++ CLI that wraps RenderDoc's replay API, enabling AI agents (and humans) to analyze GPU capture files (`.rdc`) via structured JSON output.

## Quick Start

### Prerequisites

- **Windows x64** with a GPU matching the capture's vendor
- **Visual Studio 2022** (Build Tools or full IDE)
- **CMake 3.16+**
- **RenderDoc source + built binaries** at `D:\renderdoc` (with `x64/Release/renderdoc.dll`)

### Build

```bash
cd D:\renderdoc-skill
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The executable is at `build/Release/renderdoc-tool.exe` (with `renderdoc.dll` auto-copied alongside).

### Usage

```bash
renderdoc-tool <command> <rdc-file> [options]
```

## Commands

### `open` — Validate a capture file

```bash
renderdoc-tool open capture.rdc
```
```json
{
  "status": "ok",
  "driver": "Vulkan",
  "machineIdent": "Windows x86 64-bit",
  "replaySupport": true
}
```

Lightweight check — no GPU replay needed.

### `info` — Frame summary

```bash
renderdoc-tool info capture.rdc
```
```json
{
  "api": "Vulkan",
  "frameNumber": 76,
  "totalActions": 6,
  "totalTextures": 5,
  "totalBuffers": 1
}
```

### `draws` — List all actions

```bash
renderdoc-tool draws capture.rdc
```
```json
[
  {"eventId": 11, "actionId": 1, "name": "vkCmdDraw()", "flags": ["Drawcall", "Instanced"]}
]
```

Action flags: `Drawcall`, `Clear`, `Dispatch`, `Present`, `Indexed`, `Instanced`, `BeginPass`, `EndPass`, etc.

### `event` — Pipeline state at a specific event

```bash
renderdoc-tool event capture.rdc --eid 11
```
```json
{
  "eventId": 11,
  "name": "vkCmdDraw()",
  "vertexShader": {"resourceId": 111, "entryPoint": "main"},
  "pixelShader": {"resourceId": 112, "entryPoint": "main"},
  "viewport": {"x": 0, "y": 0, "width": 500, "height": 500},
  "renderTargets": [{"resourceId": 130, "format": "R8G8B8A8_UNORM"}],
  "depthTarget": {"resourceId": 153, "format": "D16"}
}
```

## Error Handling

All errors output JSON to stdout with exit code 1:

```json
{"status": "error", "message": "Failed to open file: ..."}
```

## Project Structure

```
renderdoc-skill/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                  # Command dispatch + RenderDoc init/shutdown
│   ├── pipestate_impl.cpp        # PipeState method implementations
│   ├── commands/
│   │   ├── cmd_open.cpp
│   │   ├── cmd_info.cpp
│   │   ├── cmd_draws.cpp
│   │   └── cmd_event.cpp
│   ├── core/
│   │   ├── replay_context.cpp    # Capture file lifecycle management
│   │   └── json_output.cpp       # JSON stdout utilities
│   └── include/
│       ├── command.h             # Command base class
│       ├── replay_context.h
│       └── json_output.h
├── third_party/
│   └── nlohmann/json.hpp         # JSON library (v3.11.3)
└── .claude/
    └── skills/
        └── renderdoc-tool/
            └── SKILL.md          # Claude Code skill for auto-discovery
```

## Claude Code Integration

This project includes a Claude Code Skill (`.claude/skills/renderdoc-tool/SKILL.md`). When working in this directory, Claude Code automatically discovers the tool and can analyze `.rdc` files.

To install globally (all projects):

```bash
cp -r .claude/skills/renderdoc-tool ~/.claude/skills/
```

## Implementation Notes

- **Stateless**: Each invocation independently loads the `.rdc` file. No daemon or session.
- **PipeState workaround**: `PipeState` methods are not exported from `renderdoc.dll`. The project compiles `pipestate.inl` directly from RenderDoc source (`src/pipestate_impl.cpp`).
- **ResourceId serialization**: `ResourceId::id` is private; extracted via `memcpy` for JSON output.
- **ResultDetails::Message()**: Causes linker errors (references unexported `DoStringise`). Uses `internal_msg` field directly instead.

## License

This project links against [RenderDoc](https://github.com/baldurk/renderdoc) (MIT License).
