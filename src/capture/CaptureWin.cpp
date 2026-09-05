#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture/CaptureWin.h"

#include "core/Clock.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace ors {
namespace {

QString hrHex(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}

int overlapArea(const RECT& a, const RECT& b)
{
    const int left = std::max(static_cast<int>(a.left), static_cast<int>(b.left));
    const int top = std::max(static_cast<int>(a.top), static_cast<int>(b.top));
    const int right = std::min(static_cast<int>(a.right), static_cast<int>(b.right));
    const int bottom = std::min(static_cast<int>(a.bottom), static_cast<int>(b.bottom));
    const int x = std::max(0, right - left);
    const int y = std::max(0, bottom - top);
    return x * y;
}

RECT toRect(const CaptureSettings& settings)
{
    RECT rect{};
    rect.left = settings.x;
    rect.top = settings.y;
    rect.right = settings.x + std::max(0, settings.width);
    rect.bottom = settings.y + std::max(0, settings.height);
    return rect;
}

void blitCursor(VideoFrame& frame, int originX, int originY)
{
    if (frame.format != PixelFormat::BGRA8 || frame.width < 2 || frame.height < 2 || frame.bytes.empty()) {
        return;
    }

    CURSORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetCursorInfo(&info) || (info.flags & CURSOR_SHOWING) == 0 || info.hCursor == nullptr) {
        return;
    }

    ICONINFO iconInfo{};
    if (!GetIconInfo(info.hCursor, &iconInfo)) {
        return;
    }
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }

    const int x = info.ptScreenPos.x - static_cast<int>(iconInfo.xHotspot) - originX;
    const int y = info.ptScreenPos.y - static_cast<int>(iconInfo.yHotspot) - originY;
    if (x + 64 < 0 || y + 64 < 0 || x >= frame.width || y >= frame.height) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = frame.width;
    bmi.bmiHeader.biHeight = -frame.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) {
        return;
    }
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        DeleteDC(hdc);
        return;
    }

    const int dstStride = frame.width * 4;
    const int srcStride = frame.stride > 0 ? frame.stride : dstStride;
    auto* dest = static_cast<std::uint8_t*>(bits);
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(
            dest + static_cast<std::ptrdiff_t>(row) * dstStride,
            frame.bytes.data() + static_cast<std::ptrdiff_t>(row) * srcStride,
            static_cast<std::size_t>(dstStride));
    }

    HGDIOBJ old = SelectObject(hdc, dib);
    DrawIconEx(hdc, x, y, info.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
    SelectObject(hdc, old);

    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(
            frame.bytes.data() + static_cast<std::ptrdiff_t>(row) * srcStride,
            dest + static_cast<std::ptrdiff_t>(row) * dstStride,
            static_cast<std::size_t>(dstStride));
    }

    DeleteObject(dib);
    DeleteDC(hdc);
}

} // namespace

struct CaptureWin::Impl {
    CaptureSettings settings{};
    QString lastError;
    int width{0};
    int height{0};
    int cropX{0};
    int cropY{0};
    int outputWidth{0};
    int outputHeight{0};
    int desktopLeft{0};
    int desktopTop{0};

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> staging;
    VideoFrame lastFrame;
    bool haveLastFrame{false};

    void resetGpu()
    {
        staging.Reset();
        duplication.Reset();
        context.Reset();
        device.Reset();
        haveLastFrame = false;
        lastFrame = {};
    }

    bool createStaging(ID3D11Texture2D* source)
    {
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        staging.Reset();
        const HRESULT hr = device->CreateTexture2D(&desc, nullptr, &staging);
        if (FAILED(hr)) {
            lastError = QStringLiteral("無法建立暫存貼圖 (%1)").arg(hrHex(hr));
            return false;
        }
        outputWidth = static_cast<int>(desc.Width);
        outputHeight = static_cast<int>(desc.Height);
        return true;
    }

    bool copyMappedFrame(const D3D11_MAPPED_SUBRESOURCE& mapped, VideoFrame& out)
    {
        width = std::min(width, outputWidth - cropX) & ~1;
        height = std::min(height, outputHeight - cropY) & ~1;
        if (width < 2 || height < 2) {
            lastError = QStringLiteral("擷取區域太小");
            return false;
        }

        out.timestampNs = nowNs();
        out.width = width;
        out.height = height;
        out.stride = width * 4;
        out.format = PixelFormat::BGRA8;
        out.bytes.resize(static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(height));

        const auto* src = static_cast<const std::uint8_t*>(mapped.pData)
            + static_cast<std::ptrdiff_t>(cropY) * mapped.RowPitch
            + static_cast<std::ptrdiff_t>(cropX) * 4;
        for (int y = 0; y < height; ++y) {
            std::memcpy(
                out.bytes.data() + static_cast<std::ptrdiff_t>(y) * out.stride,
                src + static_cast<std::ptrdiff_t>(y) * mapped.RowPitch,
                static_cast<std::size_t>(width) * 4);
        }
        lastFrame = out;
        haveLastFrame = true;
        applyCursor(out);
        return true;
    }

    GrabResult copyFromResource(IDXGIResource* resource, VideoFrame& out)
    {
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
        if (FAILED(hr) || !texture) {
            lastError = QStringLiteral("無法讀取擷取貼圖");
            return GrabResult::Failed;
        }

        if (!staging && !createStaging(texture.Get())) {
            return GrabResult::Failed;
        }

        context->CopyResource(staging.Get(), texture.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            lastError = QStringLiteral("無法對應擷取貼圖 (%1)").arg(hrHex(hr));
            return GrabResult::Failed;
        }

        const bool ok = copyMappedFrame(mapped, out);
        context->Unmap(staging.Get(), 0);
        return ok ? GrabResult::Ok : GrabResult::Failed;
    }

    void applyCursor(VideoFrame& frame) const
    {
        if (!settings.includeCursor) {
            return;
        }
        blitCursor(frame, desktopLeft + cropX, desktopTop + cropY);
    }

    bool recreateDuplication(IDXGIOutput1* output1)
    {
        duplication.Reset();
        staging.Reset();
        const HRESULT hr = output1->DuplicateOutput(device.Get(), &duplication);
        if (FAILED(hr)) {
            lastError = QStringLiteral("無法啟動桌面擷取 (%1)").arg(hrHex(hr));
            return false;
        }
        return true;
    }
};

CaptureWin::CaptureWin()
    : impl_(std::make_unique<Impl>())
{}

CaptureWin::~CaptureWin()
{
    stop();
}

int CaptureWin::width() const
{
    return impl_->width;
}

int CaptureWin::height() const
{
    return impl_->height;
}

QString CaptureWin::lastError() const
{
    return impl_->lastError;
}

bool CaptureWin::start(const CaptureSettings& settings)
{
    stop();
    impl_->settings = settings;
    impl_->lastError.clear();

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立 DXGI factory (%1)").arg(hrHex(hr));
        return false;
    }

    const RECT requested = toRect(settings);
    const bool hasRegion = settings.width > 0 && settings.height > 0;
    int bestOverlap = -1;
    int index = 0;
    ComPtr<IDXGIAdapter> chosenAdapter;
    ComPtr<IDXGIOutput> chosenOutput;
    DXGI_OUTPUT_DESC chosenDesc{};

    for (UINT adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter> adapter;
        if (factory->EnumAdapters(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        for (UINT outputIndex = 0;; ++outputIndex) {
            ComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_OUTPUT_DESC desc{};
            output->GetDesc(&desc);
            if (!desc.AttachedToDesktop) {
                ++index;
                continue;
            }

            bool take = false;
            int overlap = 0;
            if (hasRegion) {
                overlap = overlapArea(requested, desc.DesktopCoordinates);
                take = overlap > bestOverlap;
            } else if (settings.monitorIndex <= 0) {
                take = chosenOutput == nullptr;
            } else {
                take = index == settings.monitorIndex;
            }

            if (take) {
                bestOverlap = overlap;
                chosenAdapter = adapter;
                chosenOutput = output;
                chosenDesc = desc;
            }
            ++index;
        }
    }

    if (!chosenAdapter || !chosenOutput) {
        impl_->lastError = QStringLiteral("找不到可用的顯示器");
        return false;
    }

    constexpr D3D_FEATURE_LEVEL kLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    hr = D3D11CreateDevice(
        chosenAdapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        kLevels,
        static_cast<UINT>(std::size(kLevels)),
        D3D11_SDK_VERSION,
        &impl_->device,
        nullptr,
        &impl_->context);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            chosenAdapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &impl_->device,
            nullptr,
            &impl_->context);
    }
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立 D3D11 裝置 (%1)").arg(hrHex(hr));
        return false;
    }

    ComPtr<IDXGIOutput1> output1;
    hr = chosenOutput.As(&output1);
    if (FAILED(hr) || !output1) {
        impl_->lastError = QStringLiteral("此顯示器不支援 Desktop Duplication");
        impl_->resetGpu();
        return false;
    }
    if (!impl_->recreateDuplication(output1.Get())) {
        impl_->resetGpu();
        return false;
    }

    const RECT& desktop = chosenDesc.DesktopCoordinates;
    const int desktopW = static_cast<int>(desktop.right - desktop.left);
    const int desktopH = static_cast<int>(desktop.bottom - desktop.top);
    impl_->desktopLeft = static_cast<int>(desktop.left);
    impl_->desktopTop = static_cast<int>(desktop.top);
    impl_->outputWidth = desktopW;
    impl_->outputHeight = desktopH;

    if (hasRegion) {
        impl_->cropX = std::clamp(settings.x - static_cast<int>(desktop.left), 0, std::max(0, desktopW - 2));
        impl_->cropY = std::clamp(settings.y - static_cast<int>(desktop.top), 0, std::max(0, desktopH - 2));
        impl_->width = std::min(settings.width, desktopW - impl_->cropX) & ~1;
        impl_->height = std::min(settings.height, desktopH - impl_->cropY) & ~1;
    } else {
        impl_->cropX = 0;
        impl_->cropY = 0;
        impl_->width = desktopW & ~1;
        impl_->height = desktopH & ~1;
    }

    if (impl_->width < 2 || impl_->height < 2) {
        impl_->lastError = QStringLiteral("擷取區域太小");
        impl_->resetGpu();
        return false;
    }

    return true;
}

void CaptureWin::stop()
{
    if (impl_->duplication) {
        impl_->duplication->ReleaseFrame();
    }
    impl_->resetGpu();
}

GrabResult CaptureWin::grab(VideoFrame& out)
{
    if (!impl_->duplication) {
        return GrabResult::Failed;
    }

    const UINT timeoutMs = impl_->settings.frameRate > 0
        ? static_cast<UINT>(std::max(8, 1000 / impl_->settings.frameRate))
        : 33;

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = impl_->duplication->AcquireNextFrame(timeoutMs, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        if (!impl_->haveLastFrame) {
            out.timestampNs = nowNs();
            out.width = impl_->width;
            out.height = impl_->height;
            out.stride = impl_->width * 4;
            out.format = PixelFormat::BGRA8;
            out.bytes.assign(static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(out.height), 0);
            impl_->lastFrame = out;
            impl_->haveLastFrame = true;
            impl_->applyCursor(out);
            return GrabResult::Ok;
        }
        if (impl_->settings.variableFrameRate) {
            return GrabResult::Idle;
        }
        out = impl_->lastFrame;
        out.timestampNs = nowNs();
        impl_->applyCursor(out);
        return GrabResult::Ok;
    }
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
        impl_->duplication.Reset();
        impl_->staging.Reset();
        impl_->lastError = QStringLiteral("桌面擷取連線中斷");
        return GrabResult::Failed;
    }
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("擷取畫面失敗 (%1)").arg(hrHex(hr));
        return GrabResult::Failed;
    }

    struct ReleaseFrame {
        IDXGIOutputDuplication* dup{};
        ~ReleaseFrame()
        {
            if (dup) {
                dup->ReleaseFrame();
            }
        }
    } release{impl_->duplication.Get()};

    return impl_->copyFromResource(resource.Get(), out);
}

GrabResult CaptureWin::grabStill(VideoFrame& out, int timeoutMs)
{
    if (!impl_->duplication) {
        impl_->lastError = QStringLiteral("尚未啟動畫面擷取");
        return GrabResult::Failed;
    }

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(std::max(1, timeoutMs));

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            impl_->lastError = QStringLiteral("截圖逾時");
            return GrabResult::Failed;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const UINT wait = static_cast<UINT>(std::max<std::int64_t>(1, remaining.count()));

        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        HRESULT hr = impl_->duplication->AcquireNextFrame(wait, &info, &resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
            impl_->duplication.Reset();
            impl_->staging.Reset();
            impl_->lastError = QStringLiteral("桌面擷取連線中斷");
            return GrabResult::Failed;
        }
        if (FAILED(hr)) {
            impl_->lastError = QStringLiteral("擷取畫面失敗 (%1)").arg(hrHex(hr));
            return GrabResult::Failed;
        }

        struct ReleaseFrame {
            IDXGIOutputDuplication* dup{};
            ~ReleaseFrame()
            {
                if (dup) {
                    dup->ReleaseFrame();
                }
            }
        } release{impl_->duplication.Get()};

        return impl_->copyFromResource(resource.Get(), out);
    }
}

} // namespace ors
