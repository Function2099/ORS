#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ors {

inline std::uint8_t clampToByte(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

// BT.601 limited-range BGRA (top-down) → NV12.
inline void convertBgraToNv12(
    const std::uint8_t* src,
    int srcStride,
    int width,
    int height,
    std::uint8_t* dst)
{
    std::uint8_t* yPlane = dst;
    std::uint8_t* uvPlane = dst + static_cast<std::ptrdiff_t>(width) * height;

    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = src + static_cast<std::ptrdiff_t>(y) * srcStride;
        std::uint8_t* yRow = yPlane + static_cast<std::ptrdiff_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            const int b = row[x * 4 + 0];
            const int g = row[x * 4 + 1];
            const int r = row[x * 4 + 2];
            yRow[x] = clampToByte(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
        }
    }

    for (int y = 0; y < height; y += 2) {
        const std::uint8_t* row0 = src + static_cast<std::ptrdiff_t>(y) * srcStride;
        const std::uint8_t* row1 = src + static_cast<std::ptrdiff_t>(y + 1) * srcStride;
        std::uint8_t* uvRow = uvPlane + static_cast<std::ptrdiff_t>(y / 2) * width;
        for (int x = 0; x < width; x += 2) {
            const int b = (row0[x * 4] + row0[(x + 1) * 4] + row1[x * 4] + row1[(x + 1) * 4]) / 4;
            const int g = (row0[x * 4 + 1] + row0[(x + 1) * 4 + 1] + row1[x * 4 + 1] + row1[(x + 1) * 4 + 1]) / 4;
            const int r = (row0[x * 4 + 2] + row0[(x + 1) * 4 + 2] + row1[x * 4 + 2] + row1[(x + 1) * 4 + 2]) / 4;
            uvRow[x] = clampToByte(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            uvRow[x + 1] = clampToByte(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
        }
    }
}

} // namespace ors
