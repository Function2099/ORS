#pragma once

#include "encode/IEncoder.h"

namespace ors {

// Phase 1: Media Foundation H.264/H.265 encoder.
class MFEncoder final : public IEncoder {
public:
    bool open(const EncoderSettings& settings) override;
    void close() override;
    bool encode(const VideoFrame& frame, EncodedPacket& out) override;
    bool flush(std::vector<EncodedPacket>& out) override;
};

} // namespace ors
