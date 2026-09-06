#include "encode/MFEncoder.h"

#include "encode/ColorConvert.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

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

bool MFEncoder::encode(VideoFrame& frame, EncodedPacket& out)
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

    if (settings_.outputFormat == PixelFormat::NV12) {
        out.bytes.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u / 2u);
        if (srcStride != width * 4 || frame.width != width || frame.height != height) {
            std::vector<std::uint8_t> packed(
                static_cast<std::size_t>(width) * 4u * static_cast<std::size_t>(height));
            for (int y = 0; y < height; ++y) {
                std::memcpy(
                    packed.data() + static_cast<std::ptrdiff_t>(y) * width * 4,
                    frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * srcStride,
                    static_cast<std::size_t>(width) * 4u);
            }
            convertBgraToNv12(packed.data(), width * 4, width, height, out.bytes.data());
        } else {
            convertBgraToNv12(frame.bytes.data(), srcStride, width, height, out.bytes.data());
        }
        frame.bytes.clear();
        return true;
    }

    const int dstStride = width * 4;
    if (srcStride == dstStride && frame.width == width && frame.height == height) {
        out.bytes = std::move(frame.bytes);
        return true;
    }

    out.bytes.resize(static_cast<std::size_t>(dstStride) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        std::memcpy(
            out.bytes.data() + static_cast<std::ptrdiff_t>(y) * dstStride,
            frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * srcStride,
            static_cast<std::size_t>(dstStride));
    }
    frame.bytes.clear();
    return true;
}

bool MFEncoder::flush(std::vector<EncodedPacket>& out)
{
    out.clear();
    return open_;
}

} // namespace ors
