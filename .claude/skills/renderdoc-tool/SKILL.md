---
name: renderdoc-tool
description: >
  Use when analyzing RenderDoc GPU capture files (.rdc), inspecting draw calls,
  examining pipeline state, debugging shaders, or exploring frame contents.
  Trigger phrases: "open .rdc", "RenderDoc capture", "GPU frame", "draw calls",
  "pipeline state", "render targets", "shader inspection", "event ID".
---

# renderdoc-tool

Stateless C++ CLI wrapping RenderDoc's replay API. Each invocation independently loads an `.rdc` file and outputs structured JSON to stdout.

**Executable:** `D:\renderdoc-skill\build\Release\renderdoc-tool.exe`

## Core Workflow

Follow this progression for any capture analysis:

1. **Validate** the file: `renderdoc-tool open capture.rdc`
2. **Overview** the frame: `renderdoc-tool info capture.rdc`
3. **List actions** to find event IDs: `renderdoc-tool draws capture.rdc`
4. **Deep-dive** into a specific event: `renderdoc-tool event capture.rdc --eid <eventId>`

Commands 2-4 require GPU replay (matching driver/vendor). Command 1 is lightweight (no replay needed).

## Command Reference

| Command | Usage | Returns |
|---------|-------|---------|
| `open` | `renderdoc-tool open <file>` | `{status, driver, machineIdent, replaySupport}` |
| `info` | `renderdoc-tool info <file>` | `{filename, api, frameNumber, totalActions, totalTextures, totalBuffers}` |
| `draws` | `renderdoc-tool draws <file>` | `[{eventId, actionId, name, flags}]` |
| `event` | `renderdoc-tool event <file> --eid N` | `{eventId, name, vertexShader, pixelShader, viewport, renderTargets, depthTarget}` |

All errors: `{"status":"error","message":"..."}` with exit code 1.

## Interpreting Output

**Action flags** (`draws` output):
- `Drawcall` = geometry draw, `Clear` = render target clear, `Dispatch` = compute shader
- `Present` = frame end, `Indexed`/`Instanced` = draw modifiers
- `BeginPass`/`EndPass` = render pass boundaries

**Pipeline state** (`event` output):
- `vertexShader`/`pixelShader`: `{resourceId, entryPoint}` or `null` (for non-draw events like Clear/Copy)
- `renderTargets`: active color attachments with format (e.g., `R8G8B8A8_UNORM`)
- `depthTarget`: depth buffer info or `null` if none bound
- `viewport`: rendering rectangle `{x, y, width, height}` in pixels

## Common Tasks

**Find all actual draw calls:**
```bash
renderdoc-tool draws capture.rdc | jq '[.[] | select(.flags | index("Drawcall"))]'
```

**Inspect pipeline at a specific draw:**
```bash
renderdoc-tool event capture.rdc --eid 11
```

**Quick frame summary:**
```bash
renderdoc-tool open capture.rdc && renderdoc-tool info capture.rdc
```

**List event IDs only:**
```bash
renderdoc-tool draws capture.rdc | jq '.[].eventId'
```
