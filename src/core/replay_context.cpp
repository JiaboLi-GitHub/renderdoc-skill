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
        if(result.internal_msg)
            msg += std::string(result.internal_msg->c_str());
        else
            msg += "error code " + std::to_string((uint32_t)result.code);
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
        if(openResult.first.internal_msg)
            msg += std::string(openResult.first.internal_msg->c_str());
        else
            msg += "error code " + std::to_string((uint32_t)openResult.first.code);
        return msg;
    }

    m_controller = openResult.second;
    return std::nullopt;
}
