#pragma once

#include <cstdint>
#include <vector>

namespace ors {

// CPU-side video frame used by FrameQueue.
// Phase 1 may attach an optional D3D11 texture on Windows without changing
// the queue API (extra fields are additive).
enum class PixelFormat {
    BGRA8,
    NV12,
};

struct VideoFrame {
    std::int64_t timestampNs{};
    int width{};
    int height{};
    int stride{};
    PixelFormat format{PixelFormat::BGRA8};
    std::vector<std::uint8_t> bytes;
};

} // namespace ors
