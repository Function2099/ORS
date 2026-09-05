#pragma once

#include "core/EncodedPacket.h"
#include "core/VideoFrame.h"

#include <QString>
#include <vector>

namespace ors {

struct EncoderSettings {
    int width{};
    int height{};
    int frameRate{30};
    int bitrateKbps{8000};
    int keyframeGopFrames{0};
    QString codec{QStringLiteral("h264")};
};

class IEncoder {
public:
    virtual ~IEncoder() = default;

    virtual bool open(const EncoderSettings& settings) = 0;
    virtual void close() = 0;
    virtual bool encode(const VideoFrame& frame, EncodedPacket& out) = 0;
    virtual bool flush(std::vector<EncodedPacket>& out) = 0;
};

} // namespace ors
