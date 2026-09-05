#pragma once

#include "audio/IAudioCapture.h"

#include <QString>
#include <memory>

namespace ors {

class AudioCaptureWin final : public IAudioCapture {
public:
    AudioCaptureWin();
    ~AudioCaptureWin() override;

    bool start(const AudioCaptureSettings& settings) override;
    void stop() override;
    bool grab(AudioFrame& out) override;
    bool grab(AudioFrame& out, int waitMs);

    int sampleRate() const;
    int channels() const;
    QString lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ors
