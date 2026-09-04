#pragma once

#include "core/VideoFrame.h"

#include <QtGlobal>

namespace ors {

enum class CaptureSource {
    Screen,
    Window,
    AudioOnly,
};

struct CaptureSettings {
    CaptureSource source{CaptureSource::Screen};
    int monitorIndex{0};
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    int frameRate{30};
    quintptr windowHandle{0};
};

class ICapture {
public:
    virtual ~ICapture() = default;

    virtual bool start(const CaptureSettings& settings) = 0;
    virtual void stop() = 0;
    // Called from the capture thread. Returns false on stop or error.
    virtual bool grab(VideoFrame& out) = 0;
};

} // namespace ors
