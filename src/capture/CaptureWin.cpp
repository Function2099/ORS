#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture/CaptureWin.h"

#include "core/Clock.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

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

bool isBgraFormat(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
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
    ComPtr<IDXGIOutput1> output1;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> staging;
    ComPtr<ID3D11Texture2D> gpuBgra;
    ComPtr<ID3D11VideoDevice> videoDevice;
    ComPtr<ID3D11VideoContext> videoContext;
    ComPtr<ID3D11VideoProcessorEnumerator> vpEnum;
    ComPtr<ID3D11VideoProcessor> videoProcessor;
    VideoFrame lastFrame;
    bool haveLastFrame{false};
    bool gdiMode{false};
    bool softwareFallback_{false};
    std::chrono::steady_clock::time_point lastGdiGrab{};
    std::chrono::steady_clock::time_point nextCfrDue{};
    bool haveCfrDue{false};

    HDC cursorDc{};
    HBITMAP cursorDib{};
    HGDIOBJ cursorOld{};
    void* cursorBits{};
    int cursorDibW{};
    int cursorDibH{};
    static constexpr UINT kAcquirePollMs = 16;

    void releaseCursorCache()
    {
        if (cursorDc) {
            if (cursorOld) {
                SelectObject(cursorDc, cursorOld);
                cursorOld = nullptr;
            }
            if (cursorDib) {
                DeleteObject(cursorDib);
                cursorDib = nullptr;
            }
            DeleteDC(cursorDc);
            cursorDc = nullptr;
        }
        cursorBits = nullptr;
        cursorDibW = 0;
        cursorDibH = 0;
    }

    void resetDuplication()
    {
        staging.Reset();
        gpuBgra.Reset();
        videoProcessor.Reset();
        vpEnum.Reset();
        videoContext.Reset();
        videoDevice.Reset();
        duplication.Reset();
    }

    void resetGpu()
    {
        resetDuplication();
        output1.Reset();
        context.Reset();
        device.Reset();
        haveLastFrame = false;
        lastFrame = {};
        lastGdiGrab = {};
        nextCfrDue = {};
        haveCfrDue = false;
        releaseCursorCache();
    }

    bool startGdiRegion()
    {
        const int w = std::max(0, settings.width) & ~1;
        const int h = std::max(0, settings.height) & ~1;
        if (w < 2 || h < 2) {
            lastError = QStringLiteral("擷取區域太小");
            return false;
        }
        desktopLeft = settings.x;
        desktopTop = settings.y;
        cropX = 0;
        cropY = 0;
        width = w;
        height = h;
        outputWidth = w;
        outputHeight = h;
        gdiMode = true;
        return true;
    }

    bool fallbackToGdi()
    {
        if (!startGdiRegion()) {
            return false;
        }
        softwareFallback_ = true;
        resetDuplication();
        output1.Reset();
        context.Reset();
        device.Reset();
        return true;
    }

    bool ensureStaging(int w, int h)
    {
        if (staging) {
            D3D11_TEXTURE2D_DESC desc{};
            staging->GetDesc(&desc);
            if (static_cast<int>(desc.Width) == w && static_cast<int>(desc.Height) == h
                && desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM) {
                return true;
            }
            staging.Reset();
        }
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(w);
        desc.Height = static_cast<UINT>(h);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        const HRESULT hr = device->CreateTexture2D(&desc, nullptr, &staging);
        if (FAILED(hr)) {
            lastError = QStringLiteral("無法建立暫存貼圖 (%1)").arg(hrHex(hr));
            return false;
        }
        return true;
    }

    bool ensureGpuBgra(int w, int h)
    {
        if (gpuBgra) {
            D3D11_TEXTURE2D_DESC desc{};
            gpuBgra->GetDesc(&desc);
            if (static_cast<int>(desc.Width) == w && static_cast<int>(desc.Height) == h) {
                return true;
            }
            gpuBgra.Reset();
            videoProcessor.Reset();
            vpEnum.Reset();
        }
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(w);
        desc.Height = static_cast<UINT>(h);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        const HRESULT hr = device->CreateTexture2D(&desc, nullptr, &gpuBgra);
        if (FAILED(hr)) {
            lastError = QStringLiteral("無法建立 BGRA 貼圖 (%1)").arg(hrHex(hr));
            return false;
        }
        return true;
    }

    bool convertToBgra(ID3D11Texture2D* source, int srcW, int srcH)
    {
        if (!ensureGpuBgra(srcW, srcH)) {
            return false;
        }
        if (!videoDevice) {
            device.As(&videoDevice);
            context.As(&videoContext);
        }
        if (!videoDevice || !videoContext) {
            return false;
        }
        if (!videoProcessor) {
            D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};
            content.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
            content.InputFrameRate.Numerator = 60;
            content.InputFrameRate.Denominator = 1;
            content.InputWidth = static_cast<UINT>(srcW);
            content.InputHeight = static_cast<UINT>(srcH);
            content.OutputFrameRate.Numerator = 60;
            content.OutputFrameRate.Denominator = 1;
            content.OutputWidth = static_cast<UINT>(srcW);
            content.OutputHeight = static_cast<UINT>(srcH);
            content.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
            HRESULT hr = videoDevice->CreateVideoProcessorEnumerator(&content, &vpEnum);
            if (FAILED(hr) || !vpEnum) {
                return false;
            }
            hr = videoDevice->CreateVideoProcessor(vpEnum.Get(), 0, &videoProcessor);
            if (FAILED(hr) || !videoProcessor) {
                vpEnum.Reset();
                return false;
            }
        }

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
        inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11VideoProcessorInputView> inView;
        HRESULT hr = videoDevice->CreateVideoProcessorInputView(
            source, vpEnum.Get(), &inDesc, &inView);
        if (FAILED(hr) || !inView) {
            return false;
        }
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
        outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11VideoProcessorOutputView> outView;
        hr = videoDevice->CreateVideoProcessorOutputView(
            gpuBgra.Get(), vpEnum.Get(), &outDesc, &outView);
        if (FAILED(hr) || !outView) {
            return false;
        }
        D3D11_VIDEO_PROCESSOR_STREAM stream{};
        stream.Enable = TRUE;
        stream.pInputSurface = inView.Get();
        hr = videoContext->VideoProcessorBlt(videoProcessor.Get(), outView.Get(), 0, 1, &stream);
        return SUCCEEDED(hr);
    }

    bool copyMappedFrame(const D3D11_MAPPED_SUBRESOURCE& mapped, VideoFrame& out, std::int64_t timestampNs)
    {
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

        const auto* src = static_cast<const std::uint8_t*>(mapped.pData);
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
        const int srcW = static_cast<int>(srcDesc.Width);
        const int srcH = static_cast<int>(srcDesc.Height);
        width = std::min(width, srcW - cropX) & ~1;
        height = std::min(height, srcH - cropY) & ~1;
        if (width < 2 || height < 2) {
            lastError = QStringLiteral("擷取區域太小");
            return GrabResult::Failed;
        }
        if (!ensureStaging(width, height)) {
            return GrabResult::Failed;
        }

        D3D11_BOX box{};
        box.left = static_cast<UINT>(cropX);
        box.top = static_cast<UINT>(cropY);
        box.front = 0;
        box.right = static_cast<UINT>(cropX + width);
        box.bottom = static_cast<UINT>(cropY + height);
        box.back = 1;

        ID3D11Texture2D* srcGpu = texture.Get();
        if (!isBgraFormat(srcDesc.Format)) {
            if (!convertToBgra(texture.Get(), srcW, srcH)) {
                lastError = QStringLiteral("無法轉換 HDR 畫面，改用 GDI");
                return GrabResult::Failed;
            }
            srcGpu = gpuBgra.Get();
        }
        context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, srcGpu, 0, &box);

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

    UINT nextAcquireTimeoutMs(bool vfr) const
    {
        if (!haveLastFrame) {
            return kAcquirePollMs;
        }
        const auto now = std::chrono::steady_clock::now();
        if (vfr) {
            if (haveCfrDue && now < nextCfrDue) {
                return static_cast<UINT>(std::max<std::int64_t>(
                    1,
                    std::chrono::duration_cast<std::chrono::milliseconds>(nextCfrDue - now).count()));
            }
            return kAcquirePollMs;
        }
        if (!haveCfrDue || now >= nextCfrDue) {
            return 1;
        }
        return static_cast<UINT>(std::max<std::int64_t>(
            1,
            std::chrono::duration_cast<std::chrono::milliseconds>(nextCfrDue - now).count()));
    }

    bool vfrDesktopTooSoon() const
    {
        if (!settings.variableFrameRate || !haveLastFrame || !haveCfrDue) {
            return false;
        }
        return std::chrono::steady_clock::now() < nextCfrDue;
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

    void applyCursor(VideoFrame& frame)
    {
        if (settings.includeCursor) {
            blitCursor(frame);
        }
    }

    bool ensureCursorDib(int w, int h)
    {
        if (cursorDc && cursorDib && cursorDibW == w && cursorDibH == h) {
            return true;
        }
        releaseCursorCache();
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        cursorDc = CreateCompatibleDC(nullptr);
        if (!cursorDc) {
            return false;
        }
        cursorDib = CreateDIBSection(cursorDc, &bmi, DIB_RGB_COLORS, &cursorBits, nullptr, 0);
        if (!cursorDib || !cursorBits) {
            releaseCursorCache();
            return false;
        }
        cursorOld = SelectObject(cursorDc, cursorDib);
        cursorDibW = w;
        cursorDibH = h;
        return true;
    }

    void blitCursor(VideoFrame& frame)
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

        const int originX = desktopLeft + cropX;
        const int originY = desktopTop + cropY;
        const int destX = info.ptScreenPos.x - static_cast<int>(iconInfo.xHotspot) - originX;
        const int destY = info.ptScreenPos.y - static_cast<int>(iconInfo.yHotspot) - originY;
        if (destX + cursorW <= 0 || destY + cursorH <= 0 || destX >= frame.width || destY >= frame.height) {
            return;
        }
        if (!ensureCursorDib(cursorW, cursorH)) {
            return;
        }

        const int srcStride = frame.stride > 0 ? frame.stride : frame.width * 4;
        const int dibStride = ((cursorW * 32 + 31) / 32) * 4;
        auto* dest = static_cast<std::uint8_t*>(cursorBits);
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

        DrawIconEx(cursorDc, 0, 0, info.hCursor, cursorW, cursorH, 0, nullptr, DI_NORMAL);

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

    GrabResult grabGdi(VideoFrame& out, bool pace, bool copyPixels)
    {
        if (pace && settings.frameRate > 0) {
            const auto interval = std::chrono::nanoseconds(
                1'000'000'000 / std::max(1, settings.frameRate));
            const auto now = std::chrono::steady_clock::now();
            if (lastGdiGrab.time_since_epoch().count() != 0) {
                const auto elapsed = now - lastGdiGrab;
                if (elapsed < interval) {
                    const auto remainMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        interval - elapsed);
                    if (remainMs.count() > 0) {
                        Sleep(static_cast<DWORD>(remainMs.count()));
                    }
                }
            }
            lastGdiGrab = std::chrono::steady_clock::now();
        }
        if (!copyPixels) {
            return GrabResult::Idle;
        }
        return blitGdiFrame(out) ? GrabResult::Ok : GrabResult::Failed;
    }

    bool recreateDuplication()
    {
        if (!output1 || !device) {
            return false;
        }
        resetDuplication();
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

bool CaptureWin::softwareFallback() const
{
    return impl_->softwareFallback_;
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
    impl_->softwareFallback_ = false;
    impl_->gdiMode = settings.captureMode.compare(QLatin1String("gdi"), Qt::CaseInsensitive) == 0;

    if (impl_->gdiMode) {
        return impl_->startGdiRegion();
    }

    auto fallback = [this]() -> bool {
        if (!impl_->fallbackToGdi()) {
            return false;
        }
        impl_->lastError.clear();
        return true;
    };

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        impl_->lastError = QStringLiteral("無法建立 DXGI factory (%1)").arg(hrHex(hr));
        return fallback();
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
        return fallback();
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
        return fallback();
    }

    hr = chosenOutput.As(&impl_->output1);
    if (FAILED(hr) || !impl_->output1) {
        impl_->lastError = QStringLiteral("此顯示器不支援 Desktop Duplication");
        impl_->resetGpu();
        return fallback();
    }
    if (!impl_->recreateDuplication()) {
        impl_->resetGpu();
        return fallback();
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
    impl_->softwareFallback_ = false;
    impl_->resetGpu();
}

GrabResult CaptureWin::grab(VideoFrame& out)
{
    return grab(out, true);
}

GrabResult CaptureWin::grab(VideoFrame& out, bool copyPixels, bool emitCachedOnIdle)
{
    if (impl_->gdiMode) {
        return impl_->grabGdi(out, true, copyPixels);
    }
    if (!impl_->duplication) {
        if (impl_->fallbackToGdi()) {
            return impl_->grabGdi(out, true, copyPixels);
        }
        return GrabResult::Failed;
    }

    const bool vfr = impl_->settings.variableFrameRate;

    const auto emitCachedIdle = [&](std::int64_t timestampNs) -> GrabResult {
        if (!copyPixels || !impl_->haveLastFrame) {
            return GrabResult::Idle;
        }
        if (vfr && !emitCachedOnIdle) {
            return GrabResult::Idle;
        }
        const GrabResult emitted = impl_->emitLastFrame(out, timestampNs);
        // VFR must not encode repeated frames; only fill lastFrame for snapshots.
        return vfr ? GrabResult::Idle : emitted;
    };

    while (true) {
        const UINT timeoutMs = impl_->nextAcquireTimeoutMs(vfr);
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        HRESULT hr = impl_->duplication->AcquireNextFrame(timeoutMs, &info, &resource);
        const std::int64_t acquiredNs = nowNs();
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return emitCachedIdle(acquiredNs);
        }
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
            if (impl_->recreateDuplication()) {
                return GrabResult::Idle;
            }
            if (impl_->fallbackToGdi()) {
                return impl_->grabGdi(out, false, copyPixels);
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

        if (!copyPixels) {
            return GrabResult::Idle;
        }

        const bool desktopUpdated = resource && info.LastPresentTime.QuadPart != 0;
        const std::int64_t timestampNs = acquiredNs;

        if (!desktopUpdated) {
            if (!impl_->haveLastFrame) {
                if (resource) {
                    const GrabResult copied = impl_->copyFromResource(resource.Get(), out, timestampNs);
                    if (copied == GrabResult::Failed && impl_->fallbackToGdi()) {
                        return impl_->grabGdi(out, false, true);
                    }
                    if (copied == GrabResult::Ok) {
                        impl_->advanceCfrDeadline();
                    }
                    return copied;
                }
                return vfr ? GrabResult::Idle : GrabResult::Failed;
            }
            return emitCachedIdle(timestampNs);
        }

        if (impl_->vfrDesktopTooSoon()) {
            continue;
        }

        const GrabResult copied = impl_->copyFromResource(resource.Get(), out, timestampNs);
        if (copied == GrabResult::Failed && impl_->fallbackToGdi()) {
            return impl_->grabGdi(out, false, true);
        }
        if (copied == GrabResult::Ok) {
            impl_->advanceCfrDeadline();
        }
        return copied;
    }

    return GrabResult::Failed;
}

GrabResult CaptureWin::grabStill(VideoFrame& out, int timeoutMs)
{
    if (impl_->gdiMode) {
        Q_UNUSED(timeoutMs);
        return impl_->grabGdi(out, false, true);
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
                forceOpaqueAlpha(out);
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
            if (impl_->recreateDuplication()) {
                continue;
            }
            impl_->duplication.Reset();
            impl_->staging.Reset();
            if (impl_->blitGdiFrame(out)) {
                forceOpaqueAlpha(out);
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

        const GrabResult copied = impl_->copyFromResource(resource.Get(), out, nowNs());
        if (copied == GrabResult::Ok) {
            forceOpaqueAlpha(out);
        }
        return copied;
    }
}

} // namespace ors
