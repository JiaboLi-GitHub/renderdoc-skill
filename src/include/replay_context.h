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
