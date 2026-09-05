#pragma once

#include "encode/IEncoder.h"

namespace ors {

// Packs captured BGRA frames for Media Foundation Sink Writer (MFVideoFormat_RGB32).
// H.264 compression itself is performed by IMFSinkWriter in Mp4Muxer.
class MFEncoder final : public IEncoder {
public:
    bool open(const EncoderSettings& settings) override;
    void close() override;
    bool encode(const VideoFrame& frame, EncodedPacket& out) override;
    bool flush(std::vector<EncodedPacket>& out) override;

private:
    EncoderSettings settings_{};
    bool open_{false};
};

} // namespace ors
