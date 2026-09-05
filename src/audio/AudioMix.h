#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace ors {

enum class MicInputSource {
    Left,
    Right,
    Stereo,
};

inline MicInputSource micInputSourceFromId(std::string_view id)
{
    if (id == "left") {
        return MicInputSource::Left;
    }
    if (id == "right") {
        return MicInputSource::Right;
    }
    return MicInputSource::Stereo;
}

inline void applyMicInputSource(
    std::int16_t* interleaved,
    std::size_t frames,
    MicInputSource source)
{
    if (interleaved == nullptr || frames == 0 || source == MicInputSource::Stereo) {
        return;
    }
    for (std::size_t i = 0; i < frames; ++i) {
        if (source == MicInputSource::Left) {
            interleaved[i * 2 + 1] = interleaved[i * 2];
        } else {
            interleaved[i * 2] = interleaved[i * 2 + 1];
        }
    }
}

inline void convertInterleavedToStereoS16(
    const std::uint8_t* data,
    std::size_t frames,
    int srcChannels,
    int bitsPerSample,
    bool asFloat,
    bool silent,
    MicInputSource source,
    std::vector<std::uint8_t>& out)
{
    const int channels = srcChannels > 0 ? srcChannels : 2;
    out.resize(frames * 2 * sizeof(std::int16_t));
    auto* dst = reinterpret_cast<std::int16_t*>(out.data());
    if (silent || data == nullptr) {
        std::memset(dst, 0, out.size());
        return;
    }

    for (std::size_t i = 0; i < frames; ++i) {
        float left = 0.0f;
        float right = 0.0f;
        if (asFloat) {
            const auto* src = reinterpret_cast<const float*>(data)
                + i * static_cast<std::size_t>(channels);
            left = src[0];
            right = channels > 1 ? src[1] : src[0];
        } else if (bitsPerSample == 16) {
            const auto* src = reinterpret_cast<const std::int16_t*>(data)
                + i * static_cast<std::size_t>(channels);
            left = src[0] / 32768.0f;
            right = channels > 1 ? src[1] / 32768.0f : left;
        } else if (bitsPerSample == 32) {
            const auto* src = reinterpret_cast<const std::int32_t*>(data)
                + i * static_cast<std::size_t>(channels);
            left = src[0] / 2147483648.0f;
            right = channels > 1 ? src[1] / 2147483648.0f : left;
        }
        left = std::clamp(left, -1.0f, 1.0f);
        right = std::clamp(right, -1.0f, 1.0f);
        dst[i * 2] = static_cast<std::int16_t>(left * 32767.0f);
        dst[i * 2 + 1] = static_cast<std::int16_t>(right * 32767.0f);
    }
    applyMicInputSource(dst, frames, source);
}

inline void mixStereoS16(std::int16_t* dst, const std::int16_t* src, std::size_t samples)
{
    if (dst == nullptr || src == nullptr || samples == 0) {
        return;
    }
    for (std::size_t i = 0; i < samples; ++i) {
        const int mixed = static_cast<int>(dst[i]) + static_cast<int>(src[i]);
        dst[i] = static_cast<std::int16_t>(std::clamp(mixed, -32768, 32767));
    }
}

inline void resampleStereoS16Linear(
    const std::int16_t* src,
    std::size_t srcFrames,
    int srcRate,
    int dstRate,
    std::vector<std::int16_t>& dst)
{
    if (src == nullptr || srcFrames == 0 || srcRate <= 0 || dstRate <= 0) {
        dst.clear();
        return;
    }
    if (srcRate == dstRate) {
        dst.assign(src, src + srcFrames * 2);
        return;
    }

    const std::size_t dstFrames = static_cast<std::size_t>(
        (static_cast<std::uint64_t>(srcFrames) * static_cast<std::uint32_t>(dstRate)
         + static_cast<std::uint32_t>(srcRate) / 2)
        / static_cast<std::uint32_t>(srcRate));
    dst.resize(dstFrames * 2);
    if (dstFrames == 0) {
        return;
    }
    for (std::size_t i = 0; i < dstFrames; ++i) {
        const double srcPos = static_cast<double>(i) * static_cast<double>(srcRate)
            / static_cast<double>(dstRate);
        auto i0 = static_cast<std::size_t>(srcPos);
        if (i0 >= srcFrames) {
            i0 = srcFrames - 1;
        }
        const std::size_t i1 = std::min(srcFrames - 1, i0 + 1);
        const float t = static_cast<float>(srcPos - static_cast<double>(i0));
        for (int ch = 0; ch < 2; ++ch) {
            const float a = static_cast<float>(src[i0 * 2 + static_cast<std::size_t>(ch)]);
            const float b = static_cast<float>(src[i1 * 2 + static_cast<std::size_t>(ch)]);
            dst[i * 2 + static_cast<std::size_t>(ch)] =
                static_cast<std::int16_t>(std::lround(a + (b - a) * t));
        }
    }
}

inline void trimStereoFifo(std::vector<std::int16_t>& fifo, std::size_t maxFrames)
{
    const std::size_t frames = fifo.size() / 2;
    if (frames <= maxFrames) {
        return;
    }
    const std::size_t drop = frames - maxFrames;
    fifo.erase(fifo.begin(), fifo.begin() + static_cast<std::ptrdiff_t>(drop * 2));
}

inline std::size_t mixStereoS16FromFifo(
    std::int16_t* dst,
    std::size_t dstFrames,
    std::vector<std::int16_t>& fifo)
{
    if (dst == nullptr || dstFrames == 0 || fifo.size() < 2) {
        return 0;
    }
    const std::size_t fifoFrames = fifo.size() / 2;
    const std::size_t mixFrames = std::min(dstFrames, fifoFrames);
    mixStereoS16(dst, fifo.data(), mixFrames * 2);
    fifo.erase(fifo.begin(), fifo.begin() + static_cast<std::ptrdiff_t>(mixFrames * 2));
    return mixFrames;
}

} // namespace ors
