# renderdoc-tool

[English](README.md)

一个无状态的 C++ 命令行工具，封装了 RenderDoc 的 Replay API，让 AI 代理（以及开发者）能够通过结构化 JSON 输出分析 GPU 抓帧文件（`.rdc`）。

## 快速开始

### 环境要求

- **Windows x64**，GPU 需与抓帧时的厂商一致
- **Visual Studio 2022**（Build Tools 或完整 IDE）
- **CMake 3.16+**
- **RenderDoc 源码 + 编译产物**，位于 `D:\renderdoc`（需要 `x64/Release/renderdoc.dll`）

### 构建

```bash
cd D:\renderdoc-skill
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

可执行文件位于 `build/Release/renderdoc-tool.exe`（`renderdoc.dll` 会自动复制到同目录）。

### 用法

```bash
renderdoc-tool <命令> <rdc文件> [选项]
```

## 命令

### `open` — 验证抓帧文件

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

轻量级检查，无需 GPU 回放。

### `info` — 帧概要信息

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

### `draws` — 列出所有操作

```bash
renderdoc-tool draws capture.rdc
```
```json
[
  {"eventId": 11, "actionId": 1, "name": "vkCmdDraw()", "flags": ["Drawcall", "Instanced"]}
]
```

操作标志：`Drawcall`（绘制调用）、`Clear`（清除）、`Dispatch`（计算着色器）、`Present`（呈现）、`Indexed`（索引绘制）、`Instanced`（实例化）、`BeginPass`/`EndPass`（渲染通道边界）等。

### `event` — 查看特定事件的管线状态

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

## 错误处理

所有错误以 JSON 格式输出到 stdout，退出码为 1：

```json
{"status": "error", "message": "Failed to open file: ..."}
```

## 项目结构

```
renderdoc-skill/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                  # 命令分发 + RenderDoc 初始化/关闭
│   ├── pipestate_impl.cpp        # PipeState 方法实现
│   ├── commands/
│   │   ├── cmd_open.cpp
│   │   ├── cmd_info.cpp
│   │   ├── cmd_draws.cpp
│   │   └── cmd_event.cpp
│   ├── core/
│   │   ├── replay_context.cpp    # 抓帧文件生命周期管理
│   │   └── json_output.cpp       # JSON 输出工具
│   └── include/
│       ├── command.h             # 命令基类
│       ├── replay_context.h
│       └── json_output.h
├── third_party/
│   └── nlohmann/json.hpp         # JSON 库 (v3.11.3)
└── .claude/
    └── skills/
        └── renderdoc-tool/
            └── SKILL.md          # Claude Code Skill 自动发现配置
```

## Claude Code 集成

本项目包含 Claude Code Skill（`.claude/skills/renderdoc-tool/SKILL.md`）。在此目录下工作时，Claude Code 会自动发现该工具并用于分析 `.rdc` 文件。

全局安装（所有项目可用）：

```bash
cp -r .claude/skills/renderdoc-tool ~/.claude/skills/
```

## 实现说明

- **无状态**：每次调用独立加载 `.rdc` 文件，无守护进程或会话。
- **PipeState 解决方案**：`PipeState` 的方法未从 `renderdoc.dll` 导出，项目直接编译 RenderDoc 源码中的 `pipestate.inl`（`src/pipestate_impl.cpp`）。
- **ResourceId 序列化**：`ResourceId::id` 是私有成员，通过 `memcpy` 提取数值用于 JSON 输出。
- **ResultDetails::Message()**：会引发链接错误（引用了未导出的 `DoStringise`），改用 `internal_msg` 字段直接访问。

## 许可证

本项目链接了 [RenderDoc](https://github.com/baldurk/renderdoc)（MIT 许可证）。
