#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "mux/Mp4Muxer.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <codecapi.h>
#include <oaidl.h>
#include <strmif.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <string>

using Microsoft::WRL::ComPtr;

namespace ors {
namespace {

QString hrHex(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}

HRESULT setFrameSize(IMFMediaType* type, UINT32 width, UINT32 height)
{
    return MFSetAttributeSize(type, MF_MT_FRAME_SIZE, width, height);
}

HRESULT setFrameRate(IMFMediaType* type, UINT32 rate)
{
    return MFSetAttributeRatio(type, MF_MT_FRAME_RATE, rate, 1);
}

HRESULT createSample(
    const std::uint8_t* data,
    DWORD size,
    std::int64_t timestampNs,
    std::int64_t durationNs,
    IMFSample** out)
{
    ComPtr<IMFSample> sample;
    HRESULT hr = MFCreateSample(&sample);
    if (FAILED(hr)) {
        return hr;
    }
    ComPtr<IMFMediaBuffer> buffer;
    hr = MFCreateMemoryBuffer(size, &buffer);
    if (FAILED(hr)) {
        return hr;
    }
    BYTE* dest = nullptr;
    DWORD maxLen = 0;
    hr = buffer->Lock(&dest, &maxLen, nullptr);
    if (FAILED(hr)) {
        return hr;
    }
    std::memcpy(dest, data, std::min<DWORD>(size, maxLen));
    buffer->Unlock();
    buffer->SetCurrentLength(size);
    sample->AddBuffer(buffer.Get());

    const LONGLONG time = std::max<std::int64_t>(0, timestampNs) / 100;
    const LONGLONG duration = std::max<std::int64_t>(1, durationNs) / 100;
    sample->SetSampleTime(time);
    sample->SetSampleDuration(duration);
    *out = sample.Detach();
    return S_OK;
}

} // namespace

struct Mp4Muxer::Impl {
    MuxerOpenParams params{};
    QString lastError;
    ComPtr<IMFSinkWriter> writer;
    DWORD videoStream{0};
    DWORD audioStream{0};
    bool hasAudio{false};
    bool writing{false};
    bool hasBase{false};
    std::int64_t baseNs{0};

    std::int64_t relativeNs(std::int64_t timestampNs)
    {
        if (!hasBase) {
            baseNs = timestampNs;
            hasBase = true;
        }
        return std::max<std::int64_t>(0, timestampNs - baseNs);
    }

    void reset()
    {
        writer.Reset();
        writing = false;
        hasBase = false;
        hasAudio = false;
        lastError.clear();
    }
};

Mp4Muxer::Mp4Muxer()
    : impl_(std::make_unique<Impl>())
{}

Mp4Muxer::~Mp4Muxer()
{
    finalize();
}

QString Mp4Muxer::lastError() const
{
    return impl_->lastError;
}

bool Mp4Muxer::open(const MuxerOpenParams& params)
{
    finalize();
    impl_->params = params;
    impl_->hasAudio = params.hasAudio && params.audioSampleRate > 0 && params.audioChannels > 0;

    if (params.filePath.isEmpty() || params.videoWidth < 2 || params.videoHeight < 2) {
        impl_->lastError = QStringLiteral("MP4 參數無效");
        return false;
    }

    ComPtr<IMFAttributes> attribs;
    HRESULT hr = MFCreateAttributes(&attribs, 4);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立 Media Foundation 屬性 (%1)").arg(hrHex(hr));
        return false;
    }
    attribs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attribs->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    attribs->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);

    const std::wstring path = params.filePath.toStdWString();
    hr = MFCreateSinkWriterFromURL(path.c_str(), nullptr, attribs.Get(), &impl_->writer);
    if (FAILED(hr)) {
        attribs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
        hr = MFCreateSinkWriterFromURL(path.c_str(), nullptr, attribs.Get(), &impl_->writer);
    }
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立 MP4 寫入器 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }

    const UINT32 width = static_cast<UINT32>(params.videoWidth & ~1);
    const UINT32 height = static_cast<UINT32>(params.videoHeight & ~1);
    const UINT32 fps = static_cast<UINT32>(std::max(1, params.videoFrameRate));
    const UINT32 bitrate = static_cast<UINT32>(std::max(1, params.videoBitrateKbps) * 1000);

    ComPtr<IMFMediaType> videoOut;
    hr = MFCreateMediaType(&videoOut);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立視訊輸出類型");
        impl_->reset();
        return false;
    }
    videoOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    videoOut->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
    videoOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    setFrameSize(videoOut.Get(), width, height);
    setFrameRate(videoOut.Get(), fps);
    MFSetAttributeRatio(videoOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (params.keyframeGopFrames > 0) {
        videoOut->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, static_cast<UINT32>(params.keyframeGopFrames));
    }
    hr = impl_->writer->AddStream(videoOut.Get(), &impl_->videoStream);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法加入 H.264 視訊軌 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }

    ComPtr<IMFMediaType> videoIn;
    hr = MFCreateMediaType(&videoIn);
    if (FAILED(hr)) {
        impl_->reset();
        return false;
    }
    videoIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    videoIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    videoIn->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    videoIn->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
    videoIn->SetUINT32(MF_MT_SAMPLE_SIZE, width * height * 4);
    videoIn->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
    setFrameSize(videoIn.Get(), width, height);
    setFrameRate(videoIn.Get(), fps);
    MFSetAttributeRatio(videoIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = impl_->writer->SetInputMediaType(impl_->videoStream, videoIn.Get(), nullptr);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法設定 RGB32 輸入 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }

    if (params.keyframeGopFrames > 0) {
        ComPtr<ICodecAPI> codec;
        if (SUCCEEDED(impl_->writer->GetServiceForStream(
                impl_->videoStream, GUID_NULL, IID_PPV_ARGS(&codec)))
            && codec) {
            VARIANT value{};
            value.vt = VT_UI4;
            value.ulVal = static_cast<ULONG>(params.keyframeGopFrames);
            codec->SetValue(&CODECAPI_AVEncMPVGOPSize, &value);
        }
    }

    if (params.encoderThreads > 0) {
        ComPtr<ICodecAPI> codec;
        if (SUCCEEDED(impl_->writer->GetServiceForStream(
                impl_->videoStream, GUID_NULL, IID_PPV_ARGS(&codec)))
            && codec) {
            VARIANT value{};
            value.vt = VT_UI4;
            value.ulVal = static_cast<ULONG>(params.encoderThreads);
            codec->SetValue(&CODECAPI_AVEncNumWorkerThreads, &value);
        }
    }

    if (impl_->hasAudio) {
        ComPtr<IMFMediaType> audioOut;
        hr = MFCreateMediaType(&audioOut);
        if (SUCCEEDED(hr)) {
            audioOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            audioOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
            audioOut->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            audioOut->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(params.audioSampleRate));
            audioOut->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, static_cast<UINT32>(params.audioChannels));
            audioOut->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
            hr = impl_->writer->AddStream(audioOut.Get(), &impl_->audioStream);
        }
        if (SUCCEEDED(hr)) {
            ComPtr<IMFMediaType> audioIn;
            hr = MFCreateMediaType(&audioIn);
            if (SUCCEEDED(hr)) {
                const UINT32 blockAlign = static_cast<UINT32>(params.audioChannels * 2);
                audioIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                audioIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                audioIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                audioIn->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(params.audioSampleRate));
                audioIn->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, static_cast<UINT32>(params.audioChannels));
                audioIn->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign);
                audioIn->SetUINT32(
                    MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                    static_cast<UINT32>(params.audioSampleRate) * blockAlign);
                hr = impl_->writer->SetInputMediaType(impl_->audioStream, audioIn.Get(), nullptr);
            }
        }
        if (FAILED(hr)) {
            impl_->hasAudio = false;
        }
    }

    hr = impl_->writer->BeginWriting();
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法開始寫入 MP4 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }
    impl_->writing = true;
    return true;
}

bool Mp4Muxer::writeVideo(const EncodedPacket& packet)
{
    if (!impl_->writing || packet.bytes.empty()) {
        return false;
    }
    ComPtr<IMFSample> sample;
    const HRESULT hr = createSample(
        packet.bytes.data(),
        static_cast<DWORD>(packet.bytes.size()),
        impl_->relativeNs(packet.timestampNs),
        packet.durationNs,
        &sample);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立視訊樣本 (%1)").arg(hrHex(hr));
        return false;
    }
    const HRESULT writeHr = impl_->writer->WriteSample(impl_->videoStream, sample.Get());
    if (FAILED(writeHr)) {
        impl_->lastError = QStringLiteral("寫入視訊失敗 (%1)").arg(hrHex(writeHr));
        return false;
    }
    return true;
}

bool Mp4Muxer::writeAudio(const EncodedPacket& packet)
{
    if (!impl_->writing || !impl_->hasAudio || packet.bytes.empty()) {
        return impl_->writing && !impl_->hasAudio;
    }
    ComPtr<IMFSample> sample;
    const HRESULT hr = createSample(
        packet.bytes.data(),
        static_cast<DWORD>(packet.bytes.size()),
        impl_->relativeNs(packet.timestampNs),
        packet.durationNs,
        &sample);
    if (FAILED(hr)) {
        return false;
    }
    return SUCCEEDED(impl_->writer->WriteSample(impl_->audioStream, sample.Get()));
}

bool Mp4Muxer::finalize()
{
    if (!impl_->writer) {
        impl_->reset();
        return true;
    }
    bool ok = true;
    if (impl_->writing) {
        const HRESULT hr = impl_->writer->Finalize();
        ok = SUCCEEDED(hr);
        if (!ok) {
            impl_->lastError = QStringLiteral("無法完成 MP4 封裝 (%1)").arg(hrHex(hr));
        }
    }
    impl_->reset();
    return ok;
}

} // namespace ors
