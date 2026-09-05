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
    bool includeCursor{true};
    bool variableFrameRate{false};
};

enum class GrabResult {
    Ok,
    Idle,
    Failed,
};

class ICapture {
public:
    virtual ~ICapture() = default;

    virtual bool start(const CaptureSettings& settings) = 0;
    virtual void stop() = 0;
    virtual GrabResult grab(VideoFrame& out) = 0;
};

} // namespace ors
