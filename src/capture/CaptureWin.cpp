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

bool isBlankBgra(const VideoFrame& frame)
{
    if (frame.format != PixelFormat::BGRA8 || frame.width < 1 || frame.height < 1 || frame.bytes.empty()) {
        return true;
    }
    const int stride = frame.stride > 0 ? frame.stride : frame.width * 4;
    for (int y = 0; y < frame.height; ++y) {
        const auto* row = frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * stride;
        for (int x = 0; x < frame.width; ++x) {
            const auto* px = row + static_cast<std::ptrdiff_t>(x) * 4;
            if ((px[0] | px[1] | px[2]) != 0) {
                return false;
            }
        }
    }
    return true;
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

void forceOpaqueAlpha(VideoFrame& frame)
{
    if (frame.format != PixelFormat::BGRA8 || frame.width < 1 || frame.height < 1 || frame.bytes.empty()) {
        return;
    }
    const int stride = frame.stride > 0 ? frame.stride : frame.width * 4;
    for (int y = 0; y < frame.height; ++y) {
        auto* row = frame.bytes.data() + static_cast<std::ptrdiff_t>(y) * stride;
        for (int x = 0; x < frame.width; ++x) {
            row[static_cast<std::ptrdiff_t>(x) * 4 + 3] = 255;
        }
    }
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

    int cursorW = 32;
    int cursorH = 32;
    BITMAP bitmap{};
    if (iconInfo.hbmColor && GetObject(iconInfo.hbmColor, sizeof(bitmap), &bitmap)) {
        cursorW = std::max(1, static_cast<int>(bitmap.bmWidth));
        cursorH = std::max(1, static_cast<int>(bitmap.bmHeight));
    } else if (iconInfo.hbmMask && GetObject(iconInfo.hbmMask, sizeof(bitmap), &bitmap)) {
        cursorW = std::max(1, static_cast<int>(bitmap.bmWidth));
        cursorH = std::max(1, static_cast<int>(bitmap.bmHeight) / 2);
    }
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }

    const int destX = info.ptScreenPos.x - static_cast<int>(iconInfo.xHotspot) - originX;
    const int destY = info.ptScreenPos.y - static_cast<int>(iconInfo.yHotspot) - originY;
    if (destX + cursorW <= 0 || destY + cursorH <= 0 || destX >= frame.width || destY >= frame.height) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cursorW;
    bmi.bmiHeader.biHeight = -cursorH;
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

    const int srcStride = frame.stride > 0 ? frame.stride : frame.width * 4;
    const int dibStride = ((cursorW * 32 + 31) / 32) * 4;
    auto* dest = static_cast<std::uint8_t*>(bits);
    std::memset(dest, 0, static_cast<std::size_t>(dibStride) * static_cast<std::size_t>(cursorH));

    for (int row = 0; row < cursorH; ++row) {
        const int fy = destY + row;
        if (fy < 0 || fy >= frame.height) {
            continue;
        }
        const int x0 = std::max(0, destX);
        const int x1 = std::min(frame.width, destX + cursorW);
        if (x0 >= x1) {
            continue;
        }
        std::memcpy(
            dest + static_cast<std::ptrdiff_t>(row) * dibStride
                + static_cast<std::ptrdiff_t>(x0 - destX) * 4,
            frame.bytes.data() + static_cast<std::ptrdiff_t>(fy) * srcStride
                + static_cast<std::ptrdiff_t>(x0) * 4,
            static_cast<std::size_t>(x1 - x0) * 4);
    }

    HGDIOBJ old = SelectObject(hdc, dib);
    DrawIconEx(hdc, 0, 0, info.hCursor, cursorW, cursorH, 0, nullptr, DI_NORMAL);
    SelectObject(hdc, old);

    for (int row = 0; row < cursorH; ++row) {
        const int fy = destY + row;
        if (fy < 0 || fy >= frame.height) {
            continue;
        }
        const int x0 = std::max(0, destX);
        const int x1 = std::min(frame.width, destX + cursorW);
        if (x0 >= x1) {
            continue;
        }
        std::memcpy(
            frame.bytes.data() + static_cast<std::ptrdiff_t>(fy) * srcStride
                + static_cast<std::ptrdiff_t>(x0) * 4,
            dest + static_cast<std::ptrdiff_t>(row) * dibStride
                + static_cast<std::ptrdiff_t>(x0 - destX) * 4,
            static_cast<std::size_t>(x1 - x0) * 4);
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
    bool gdiMode{false};
    std::chrono::steady_clock::time_point lastGdiGrab{};
    std::chrono::steady_clock::time_point nextCfrDue{};
    bool haveCfrDue{false};
    int consecutiveBlank_{0};

    void resetGpu()
    {
        staging.Reset();
        duplication.Reset();
        context.Reset();
        device.Reset();
        haveLastFrame = false;
        lastFrame = {};
        lastGdiGrab = {};
        nextCfrDue = {};
        haveCfrDue = false;
        consecutiveBlank_ = 0;
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

    bool copyMappedFrame(const D3D11_MAPPED_SUBRESOURCE& mapped, VideoFrame& out, std::int64_t timestampNs)
    {
        width = std::min(width, outputWidth - cropX) & ~1;
        height = std::min(height, outputHeight - cropY) & ~1;
        if (width < 2 || height < 2) {
            lastError = QStringLiteral("擷取區域太小");
            return false;
        }

        out.timestampNs = timestampNs != 0 ? timestampNs : nowNs();
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

        // DWM + WDA_EXCLUDEFROMCAPTURE can insert a one-frame black present.
        if (isBlankBgra(out)) {
            ++consecutiveBlank_;
            if (haveLastFrame && consecutiveBlank_ == 1) {
                out = lastFrame;
                out.timestampNs = timestampNs != 0 ? timestampNs : nowNs();
                applyCursor(out);
                return true;
            }
        } else {
            consecutiveBlank_ = 0;
        }

        lastFrame = out;
        haveLastFrame = true;
        applyCursor(out);
        return true;
    }

    GrabResult copyFromResource(IDXGIResource* resource, VideoFrame& out, std::int64_t timestampNs)
    {
        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
        if (FAILED(hr) || !texture) {
            lastError = QStringLiteral("無法讀取擷取貼圖");
            return GrabResult::Failed;
        }

        D3D11_TEXTURE2D_DESC srcDesc{};
        texture->GetDesc(&srcDesc);
        if (staging) {
            D3D11_TEXTURE2D_DESC stageDesc{};
            staging->GetDesc(&stageDesc);
            if (stageDesc.Width != srcDesc.Width || stageDesc.Height != srcDesc.Height
                || stageDesc.Format != srcDesc.Format) {
                staging.Reset();
            }
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

        const bool ok = copyMappedFrame(mapped, out, timestampNs);
        context->Unmap(staging.Get(), 0);
        return ok ? GrabResult::Ok : GrabResult::Failed;
    }

    void advanceCfrDeadline()
    {
        const auto interval = std::chrono::nanoseconds(
            1'000'000'000 / std::max(1, settings.frameRate));
        const auto now = std::chrono::steady_clock::now();
        if (!haveCfrDue) {
            nextCfrDue = now + interval;
            haveCfrDue = true;
            return;
        }
        nextCfrDue += interval;
        if (nextCfrDue < now) {
            nextCfrDue = now + interval;
        }
    }

    GrabResult emitLastFrame(VideoFrame& out, std::int64_t timestampNs)
    {
        out = lastFrame;
        out.timestampNs = timestampNs;
        applyCursor(out);
        if (!settings.variableFrameRate) {
            advanceCfrDeadline();
        }
        return GrabResult::Ok;
    }

    void applyCursor(VideoFrame& frame) const
    {
        forceOpaqueAlpha(frame);
        if (settings.includeCursor) {
            blitCursor(frame, desktopLeft + cropX, desktopTop + cropY);
        }
    }

    bool blitGdiFrame(VideoFrame& out)
    {
        width = std::max(2, width & ~1);
        height = std::max(2, height & ~1);

        HDC screen = GetDC(nullptr);
        if (!screen) {
            lastError = QStringLiteral("無法取得螢幕裝置內容");
            return false;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC mem = CreateCompatibleDC(screen);
        if (!mem) {
            ReleaseDC(nullptr, screen);
            lastError = QStringLiteral("無法建立 GDI 暫存 DC");
            return false;
        }
        HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            DeleteDC(mem);
            ReleaseDC(nullptr, screen);
            lastError = QStringLiteral("無法建立 GDI 擷取緩衝");
            return false;
        }

        HGDIOBJ old = SelectObject(mem, dib);
        const int srcX = desktopLeft + cropX;
        const int srcY = desktopTop + cropY;
        DWORD rop = SRCCOPY | CAPTUREBLT;
        if (!BitBlt(mem, 0, 0, width, height, screen, srcX, srcY, rop)) {
            BitBlt(mem, 0, 0, width, height, screen, srcX, srcY, SRCCOPY);
        }
        SelectObject(mem, old);

        out.timestampNs = nowNs();
        out.width = width;
        out.height = height;
        out.stride = width * 4;
        out.format = PixelFormat::BGRA8;
        out.bytes.resize(static_cast<std::size_t>(out.stride) * static_cast<std::size_t>(height));
        const int dibStride = ((width * 32 + 31) / 32) * 4;
        const auto* src = static_cast<const std::uint8_t*>(bits);
        for (int y = 0; y < height; ++y) {
            auto* dst = out.bytes.data() + static_cast<std::ptrdiff_t>(y) * out.stride;
            std::memcpy(
                dst,
                src + static_cast<std::ptrdiff_t>(y) * dibStride,
                static_cast<std::size_t>(width) * 4);
        }

        DeleteObject(dib);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);

        lastFrame = out;
        haveLastFrame = true;
        applyCursor(out);
        return true;
    }

    GrabResult grabGdi(VideoFrame& out, bool pace)
    {
        if (pace && settings.frameRate > 0) {
            const auto interval = std::chrono::milliseconds(std::max(8, 1000 / settings.frameRate));
            const auto now = std::chrono::steady_clock::now();
            if (lastGdiGrab.time_since_epoch().count() != 0) {
                const auto elapsed = now - lastGdiGrab;
                if (elapsed < interval) {
                    Sleep(static_cast<DWORD>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(interval - elapsed).count()));
                }
            }
            lastGdiGrab = std::chrono::steady_clock::now();
        }
        return blitGdiFrame(out) ? GrabResult::Ok : GrabResult::Failed;
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
    impl_->gdiMode = settings.captureMode.compare(QLatin1String("gdi"), Qt::CaseInsensitive) == 0;

    if (impl_->gdiMode) {
        const int w = std::max(0, settings.width) & ~1;
        const int h = std::max(0, settings.height) & ~1;
        if (w < 2 || h < 2) {
            impl_->lastError = QStringLiteral("擷取區域太小");
            impl_->gdiMode = false;
            return false;
        }
        impl_->desktopLeft = settings.x;
        impl_->desktopTop = settings.y;
        impl_->cropX = 0;
        impl_->cropY = 0;
        impl_->width = w;
        impl_->height = h;
        impl_->outputWidth = w;
        impl_->outputHeight = h;
        return true;
    }

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
    impl_->gdiMode = false;
    impl_->resetGpu();
}

GrabResult CaptureWin::grab(VideoFrame& out)
{
    if (impl_->gdiMode) {
        return impl_->grabGdi(out, true);
    }
    if (!impl_->duplication) {
        return GrabResult::Failed;
    }

    const bool vfr = impl_->settings.variableFrameRate;
    UINT timeoutMs = 200;
    if (!vfr && impl_->haveLastFrame) {
        const auto now = std::chrono::steady_clock::now();
        if (!impl_->haveCfrDue) {
            timeoutMs = 1;
        } else if (now >= impl_->nextCfrDue) {
            timeoutMs = 1;
        } else {
            timeoutMs = static_cast<UINT>(std::max<std::int64_t>(
                1,
                std::chrono::duration_cast<std::chrono::milliseconds>(impl_->nextCfrDue - now).count()));
        }
    }

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = impl_->duplication->AcquireNextFrame(timeoutMs, &info, &resource);
    const std::int64_t acquiredNs = nowNs();
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        if (!impl_->haveLastFrame || vfr) {
            return GrabResult::Idle;
        }
        return impl_->emitLastFrame(out, acquiredNs);
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

    const bool desktopUpdated = resource && info.LastPresentTime.QuadPart != 0;
    const std::int64_t timestampNs = acquiredNs;

    if (!desktopUpdated) {
        if (!impl_->haveLastFrame) {
            if (resource) {
                const GrabResult copied = impl_->copyFromResource(resource.Get(), out, timestampNs);
                if (copied == GrabResult::Ok && !vfr) {
                    impl_->advanceCfrDeadline();
                }
                return copied;
            }
            return vfr ? GrabResult::Idle : GrabResult::Failed;
        }
        if (vfr && !impl_->settings.includeCursor) {
            return GrabResult::Idle;
        }
        return impl_->emitLastFrame(out, timestampNs);
    }

    const GrabResult copied = impl_->copyFromResource(resource.Get(), out, timestampNs);
    if (copied == GrabResult::Ok && !vfr) {
        impl_->advanceCfrDeadline();
    }
    return copied;
}

GrabResult CaptureWin::grabStill(VideoFrame& out, int timeoutMs)
{
    if (impl_->gdiMode) {
        Q_UNUSED(timeoutMs);
        return impl_->grabGdi(out, false);
    }
    if (!impl_->duplication) {
        impl_->lastError = QStringLiteral("尚未啟動畫面擷取");
        return GrabResult::Failed;
    }

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(std::max(1, timeoutMs));

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            if (impl_->blitGdiFrame(out)) {
                impl_->lastError.clear();
                return GrabResult::Ok;
            }
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
            if (impl_->blitGdiFrame(out)) {
                impl_->lastError.clear();
                return GrabResult::Ok;
            }
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

        if (!resource || info.LastPresentTime.QuadPart == 0) {
            continue;
        }

        return impl_->copyFromResource(resource.Get(), out, nowNs());
    }
}

} // namespace ors
