#include "encode/MFEncoder.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace ors {

bool MFEncoder::open(const EncoderSettings& settings)
{
    if (settings.width < 2 || settings.height < 2 || settings.frameRate <= 0) {
        return false;
    }
    settings_ = settings;
    settings_.width &= ~1;
    settings_.height &= ~1;
    open_ = true;
    return true;
}

void MFEncoder::close()
{
    open_ = false;
}

bool MFEncoder::encode(const VideoFrame& frame, EncodedPacket& out)
{
    if (!open_ || frame.format != PixelFormat::BGRA8 || frame.width < 2 || frame.height < 2
        || frame.bytes.empty()) {
        return false;
    }

    const int width = std::min(frame.width, settings_.width) & ~1;
    const int height = std::min(frame.height, settings_.height) & ~1;
    if (width < 2 || height < 2) {
        return false;
    }

    out.kind = PacketKind::Video;
    out.timestampNs = frame.timestampNs;
    out.durationNs = 1'000'000'000 / std::max(1, settings_.frameRate);
    out.keyframe = false;

    const int srcStride = frame.stride > 0 ? frame.stride : frame.width * 4;
    const int dstStride = width * 4;
    out.bytes.resize(static_cast<std::size_t>(dstStride) * static_cast<std::size_t>(height));

    if (srcStride == dstStride && frame.width == width && frame.height == height) {
        std::memcpy(out.bytes.data(), frame.bytes.data(), out.bytes.size());
        return true;
    }

    for (int y = 0; y < height; ++y) {
        std::memcpy(
            out.bytes.data() + static_cast<std::ptrdiff_t>(y) * dstStride,
            frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * srcStride,
            static_cast<std::size_t>(dstStride));
    }
    return true;
}

bool MFEncoder::flush(std::vector<EncodedPacket>& out)
{
    out.clear();
    return open_;
}

} // namespace ors
