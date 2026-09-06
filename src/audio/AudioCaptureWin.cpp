#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "audio/AudioCaptureWin.h"

#include "audio/AudioDevices.h"
#include "audio/AudioMix.h"
#include "core/Clock.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <propidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace ors {
namespace {

QString hrHex(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}

struct WaveFormatDeleter {
    void operator()(WAVEFORMATEX* format) const
    {
        if (format) {
            CoTaskMemFree(format);
        }
    }
};

bool isFloatFormat(const WAVEFORMATEX* format)
{
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        static const GUID kIeeeFloat{
            0x00000003,
            0x0000,
            0x0010,
            {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        return ext->SubFormat == kIeeeFloat;
    }
    return false;
}

QString friendlyName(IMMDevice* device)
{
    if (!device) {
        return {};
    }
    ComPtr<IPropertyStore> props;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props)) || !props) {
        return {};
    }
    // PKEY_Device_FriendlyName
    static const PROPERTYKEY kFriendlyName{
        {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
        14};
    PROPVARIANT value;
    PropVariantInit(&value);
    QString name;
    if (SUCCEEDED(props->GetValue(kFriendlyName, &value)) && value.vt == VT_LPWSTR
        && value.pwszVal) {
        name = QString::fromWCharArray(value.pwszVal);
    }
    PropVariantClear(&value);
    return name;
}

} // namespace

struct AudioCaptureWin::Impl {
    QString lastError;
    int sampleRate{48000};
    int channels{2};
    bool loopback{false};
    MicInputSource inputSource{MicInputSource::Stereo};
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    std::unique_ptr<WAVEFORMATEX, WaveFormatDeleter> mixFormat;
    HANDLE event{nullptr};
    bool useEvent{false};
    bool haveQpcOffset{false};
    std::int64_t qpcOffsetNs{0};

    void closeEventHandle()
    {
        if (event) {
            CloseHandle(event);
            event = nullptr;
        }
        useEvent = false;
    }

    void reset()
    {
        if (client) {
            client->Stop();
        }
        capture.Reset();
        client.Reset();
        device.Reset();
        mixFormat.reset();
        closeEventHandle();
        haveQpcOffset = false;
        qpcOffsetNs = 0;
    }
};

AudioCaptureWin::AudioCaptureWin()
    : impl_(std::make_unique<Impl>())
{}

AudioCaptureWin::~AudioCaptureWin()
{
    stop();
}

int AudioCaptureWin::sampleRate() const
{
    return impl_->sampleRate;
}

int AudioCaptureWin::channels() const
{
    return impl_->channels;
}

QString AudioCaptureWin::lastError() const
{
    return impl_->lastError;
}

bool AudioCaptureWin::start(const AudioCaptureSettings& settings)
{
    stop();
    impl_->lastError.clear();
    impl_->loopback = settings.systemAudio;
    impl_->channels = 2;
    impl_->inputSource = impl_->loopback
        ? MicInputSource::Stereo
        : micInputSourceFromId(settings.inputSource.toUtf8().constData());

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立音訊裝置列舉器 (%1)").arg(hrHex(hr));
        return false;
    }

    if (impl_->loopback) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device);
    } else if (!settings.microphoneId.isEmpty()) {
        hr = enumerator->GetDevice(
            reinterpret_cast<LPCWSTR>(settings.microphoneId.utf16()), &impl_->device);
        if (FAILED(hr) && settings.microphoneId == QLatin1String("default")) {
            hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &impl_->device);
        }
    } else {
        hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &impl_->device);
    }
    if (FAILED(hr) || !impl_->device) {
        impl_->lastError = QStringLiteral("找不到音訊裝置 (%1)").arg(hrHex(hr));
        return false;
    }

    hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &impl_->client);
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法啟動音訊用戶端 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }

    WAVEFORMATEX* rawFormat = nullptr;
    hr = impl_->client->GetMixFormat(&rawFormat);
    if (FAILED(hr) || rawFormat == nullptr) {
        impl_->lastError = QStringLiteral("無法讀取音訊格式 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }
    impl_->mixFormat.reset(rawFormat);
    impl_->sampleRate = static_cast<int>(impl_->mixFormat->nSamplesPerSec);

    DWORD flags = 0;
    if (impl_->loopback) {
        flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }
    constexpr REFERENCE_TIME kBuffer100ns = 2000000; // 200 ms
    impl_->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DWORD initFlags = flags;
    if (impl_->event) {
        initFlags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    }
    hr = impl_->client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        initFlags,
        kBuffer100ns,
        0,
        impl_->mixFormat.get(),
        nullptr);
    if (FAILED(hr) && impl_->event) {
        impl_->closeEventHandle();
        impl_->client.Reset();
        hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &impl_->client);
        if (SUCCEEDED(hr)) {
            hr = impl_->client->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                flags,
                kBuffer100ns,
                0,
                impl_->mixFormat.get(),
                nullptr);
        }
    }
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法初始化 WASAPI (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }
    if (impl_->event) {
        hr = impl_->client->SetEventHandle(impl_->event);
        if (SUCCEEDED(hr)) {
            impl_->useEvent = true;
        } else {
            impl_->closeEventHandle();
            impl_->client.Reset();
            hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &impl_->client);
            if (SUCCEEDED(hr)) {
                hr = impl_->client->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    flags,
                    kBuffer100ns,
                    0,
                    impl_->mixFormat.get(),
                    nullptr);
            }
            if (FAILED(hr)) {
                impl_->lastError = QStringLiteral("無法初始化 WASAPI (%1)").arg(hrHex(hr));
                impl_->reset();
                return false;
            }
        }
    }

    hr = impl_->client->GetService(IID_PPV_ARGS(&impl_->capture));
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法取得音訊擷取介面 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }

    hr = impl_->client->Start();
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法開始音訊擷取 (%1)").arg(hrHex(hr));
        impl_->reset();
        return false;
    }
    return true;
}

void AudioCaptureWin::stop()
{
    impl_->reset();
}

bool AudioCaptureWin::grab(AudioFrame& out)
{
    return grab(out, 16);
}

bool AudioCaptureWin::grab(AudioFrame& out, int waitMs)
{
    if (!impl_->capture) {
        return false;
    }

    const int attempts = waitMs <= 0 ? 1 : (impl_->useEvent ? 2 : std::max(1, waitMs / 5));
    for (int attempt = 0; attempt < attempts; ++attempt) {
        UINT32 packet = 0;
        HRESULT hr = impl_->capture->GetNextPacketSize(&packet);
        if (FAILED(hr)) {
            impl_->lastError = QStringLiteral("讀取音訊封包失敗 (%1)").arg(hrHex(hr));
            return false;
        }
        if (packet == 0) {
            if (waitMs <= 0) {
                break;
            }
            if (impl_->useEvent && impl_->event) {
                const DWORD wr = WaitForSingleObject(impl_->event, static_cast<DWORD>(waitMs));
                if (wr != WAIT_OBJECT_0) {
                    break;
                }
                continue;
            }
            Sleep(5);
            continue;
        }

        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        UINT64 qpc = 0;
        hr = impl_->capture->GetBuffer(&data, &frames, &flags, nullptr, &qpc);
        if (FAILED(hr)) {
            impl_->lastError = QStringLiteral("讀取音訊緩衝失敗 (%1)").arg(hrHex(hr));
            return false;
        }

        convertInterleavedToStereoS16(
            data,
            frames,
            impl_->mixFormat->nChannels > 0 ? impl_->mixFormat->nChannels : 2,
            impl_->mixFormat->wBitsPerSample,
            isFloatFormat(impl_->mixFormat.get()),
            (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0,
            impl_->inputSource,
            out.bytes);
        impl_->capture->ReleaseBuffer(frames);

        if (qpc != 0) {
            const std::int64_t packetNs = static_cast<std::int64_t>(qpc) * 100;
            if (!impl_->haveQpcOffset) {
                impl_->qpcOffsetNs = nowNs() - packetNs;
                impl_->haveQpcOffset = true;
            }
            out.timestampNs = packetNs + impl_->qpcOffsetNs;
        } else {
            out.timestampNs = nowNs();
        }
        out.sampleRate = impl_->sampleRate;
        out.channels = 2;
        out.bitsPerSample = 16;
        return true;
    }

    out.timestampNs = nowNs();
    out.sampleRate = impl_->sampleRate;
    out.channels = 2;
    out.bitsPerSample = 16;
    out.bytes.clear();
    return true;
}

QVector<AudioDeviceInfo> listCaptureDevices()
{
    QVector<AudioDeviceInfo> listed;
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(comHr);
    if (FAILED(comHr) && comHr != RPC_E_CHANGED_MODE) {
        return listed;
    }
    struct ComGuard {
        bool uninit{};
        ~ComGuard()
        {
            if (uninit) {
                CoUninitialize();
            }
        }
    } guard{uninit};

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) {
        return listed;
    }

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) {
        return listed;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device)) || !device) {
            continue;
        }
        LPWSTR id = nullptr;
        if (FAILED(device->GetId(&id)) || id == nullptr) {
            continue;
        }
        AudioDeviceInfo info;
        info.id = QString::fromWCharArray(id);
        CoTaskMemFree(id);
        info.name = friendlyName(device.Get());
        if (info.name.isEmpty()) {
            info.name = info.id;
        }
        listed.push_back(std::move(info));
    }
    return listed;
}

} // namespace ors
