#pragma once

#include "core/EncodedPacket.h"
#include "core/VideoFrame.h"

#include <QString>

namespace ors {

struct MuxerOpenParams {
    QString filePath;
    int videoWidth{};
    int videoHeight{};
    int videoFrameRate{30};
    int videoBitrateKbps{8000};
    int keyframeGopFrames{0};
    int encoderThreads{0};
    int audioSampleRate{48000};
    int audioChannels{2};
    bool hasAudio{false};
    bool preferNv12{true};
};

class IMuxer {
public:
    virtual ~IMuxer() = default;

    virtual bool open(const MuxerOpenParams& params) = 0;
    virtual bool writeVideo(const EncodedPacket& packet) = 0;
    virtual bool writeAudio(const EncodedPacket& packet) = 0;
    virtual bool finalize() = 0;
    virtual QString lastError() const = 0;
    virtual PixelFormat videoInputFormat() const { return PixelFormat::BGRA8; }
    virtual bool hardwareVideoEncoder() const { return true; }
};

} // namespace ors
