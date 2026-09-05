#pragma once

#include "core/AudioFrame.h"

#include <QString>

namespace ors {

struct AudioCaptureSettings {
    bool systemAudio{true};
    QString microphoneId;
    QString inputSource{QStringLiteral("stereo")};
    int sampleRate{48000};
    int channels{2};
};

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    virtual bool start(const AudioCaptureSettings& settings) = 0;
    virtual void stop() = 0;
    // Called from the audio capture thread. Returns false on stop or error.
    virtual bool grab(AudioFrame& out) = 0;
};

} // namespace ors
