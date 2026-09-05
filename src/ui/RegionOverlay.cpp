#include "ui/RegionOverlay.h"

#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace ors {
namespace {

constexpr int kHit = 8;
constexpr int kHandle = 7;
constexpr int kChipW = 118;
constexpr int kChipH = 22;
constexpr int kChipPad = 4;

QRect dipVirtualDesktop()
{
    if (QScreen* primary = QGuiApplication::primaryScreen()) {
        return primary->virtualGeometry();
    }
    return {0, 0, 1920, 1080};
}

QRect normalized(const QPoint& a, const QPoint& b)
{
    return QRect(a, b).normalized();
}

#ifdef Q_OS_WIN
struct MonitorMatch {
    QString name;
    QRect rect;
    bool found{false};
};

BOOL CALLBACK matchMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM param)
{
    auto* match = reinterpret_cast<MonitorMatch*>(param);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return TRUE;
    }
    if (QString::fromWCharArray(info.szDevice).compare(match->name, Qt::CaseInsensitive) != 0) {
        return TRUE;
    }
    match->rect = QRect(
        static_cast<int>(info.rcMonitor.left),
        static_cast<int>(info.rcMonitor.top),
        static_cast<int>(info.rcMonitor.right - info.rcMonitor.left),
        static_cast<int>(info.rcMonitor.bottom - info.rcMonitor.top));
    match->found = true;
    return FALSE;
}
#endif

QRect scaledFromDip(const QRect& dip, qreal dpr)
{
    return QRect(
        qRound(dip.x() * dpr),
        qRound(dip.y() * dpr),
        qMax(1, qRound(dip.width() * dpr)),
        qMax(1, qRound(dip.height() * dpr)));
}

QRect mapAcross(const QRect& source, const QRect& from, const QRect& to)
{
    if (from.width() <= 0 || from.height() <= 0 || to.width() <= 0 || to.height() <= 0) {
        return source;
    }
    const int x = to.x() + qRound(double(source.x() - from.x()) * to.width() / from.width());
    const int y = to.y() + qRound(double(source.y() - from.y()) * to.height() / from.height());
    const int w = qMax(1, qRound(double(source.width()) * to.width() / from.width()));
    const int h = qMax(1, qRound(double(source.height()) * to.height() / from.height()));
    return {x, y, w, h};
}

QScreen* screenForDip(const QPoint& dipPos)
{
    if (QScreen* screen = QGuiApplication::screenAt(dipPos)) {
        return screen;
    }
    return QGuiApplication::primaryScreen();
}

QScreen* screenForNative(const QPoint& nativePos)
{
    QScreen* best = QGuiApplication::primaryScreen();
    for (QScreen* screen : QGuiApplication::screens()) {
        if (nativeScreenGeometry(screen).contains(nativePos)) {
            return screen;
        }
    }
    return best;
}

QRect nativeToDipRect(const QRect& native, QScreen* screen)
{
    if (!screen) {
        screen = screenForNative(native.center());
    }
    if (!screen) {
        return native;
    }
    const QRect ns = nativeScreenGeometry(screen);
    const QRect ds = screen->geometry();
    if (ns.width() <= 0 || ns.height() <= 0) {
        const qreal dpr = screen->devicePixelRatio();
        return QRect(
            qRound(native.x() / dpr),
            qRound(native.y() / dpr),
            qMax(1, qRound(native.width() / dpr)),
            qMax(1, qRound(native.height() / dpr)));
    }
    return mapAcross(native, ns, ds);
}

QRect dipToNativeRect(const QRect& dip, QScreen* screen)
{
    if (!screen) {
        screen = screenForDip(dip.center());
    }
    if (!screen) {
        return dip;
    }
    const QRect ds = screen->geometry();
    const QRect ns = nativeScreenGeometry(screen);
    if (ns.width() <= 0 || ns.height() <= 0) {
        return scaledFromDip(dip, screen->devicePixelRatio());
    }
    return mapAcross(dip, ds, ns);
}

} // namespace

QRect nativeVirtualDesktop()
{
#ifdef Q_OS_WIN
    return QRect(
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_CYVIRTUALSCREEN));
#else
    return dipVirtualDesktop();
#endif
}

QRect nativeScreenGeometry(QScreen* screen)
{
    if (!screen) {
        return nativeVirtualDesktop();
    }
#ifdef Q_OS_WIN
    MonitorMatch match;
    match.name = screen->name();
    EnumDisplayMonitors(nullptr, nullptr, matchMonitor, reinterpret_cast<LPARAM>(&match));
    if (match.found && match.rect.isValid()) {
        return match.rect;
    }
#endif
    return scaledFromDip(screen->geometry(), screen->devicePixelRatio());
}

RegionOverlay::RegionOverlay(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(kMinWidth, kMinHeight);
    createWinId();
    applyNativeWindowHints();
}

QRect RegionOverlay::captureRect() const
{
    return captureRectNative();
}

QRect RegionOverlay::captureRectNative() const
{
#ifdef Q_OS_WIN
    if (internalWinId()) {
        RECT wr{};
        if (GetWindowRect(reinterpret_cast<HWND>(internalWinId()), &wr)) {
            QRect native(wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);
            if (native.width() >= 2 && native.height() >= 2) {
                return native;
            }
        }
    }
#endif
    if (nativeRect_.isValid()) {
        return nativeRect_;
    }
    return dipToNativeRect(geometry(), screen());
}

void RegionOverlay::setCaptureRect(const QRect& nativeRect)
{
    const QRect native = evenSize(clampNative(nativeRect));
    if (native.isEmpty()) {
        return;
    }
    if (nativeRect_.isValid() && native == nativeRect_ && native == captureRectNative()) {
        return;
    }

    nativeRect_ = native;
    applyingNative_ = true;
    setGeometry(nativeToDipRect(native, screenForNative(native.center())));
    applyWin32Bounds();
    applyingNative_ = false;
    emitNativeChanged();
}

void RegionOverlay::setInteractive(bool enabled)
{
    interactive_ = enabled;
    if (!interactive_) {
        dragging_ = false;
        hit_ = HitZone::None;
        unsetCursor();
    }
    update();
}

void RegionOverlay::setRecording(bool recording)
{
    recording_ = recording;
    update();
}

void RegionOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyNativeWindowHints();
    applyWin32Bounds();
}

void RegionOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!applyingNative_ && !dragging_ && nativeRect_.isValid()) {
        applyWin32Bounds();
    }
}

void RegionOverlay::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    if (!applyingNative_ && !dragging_ && nativeRect_.isValid()) {
        applyWin32Bounds();
    }
}

void RegionOverlay::applyNativeWindowHints()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
#endif
}

void RegionOverlay::applyWin32Bounds()
{
#ifdef Q_OS_WIN
    if (!nativeRect_.isValid() || !internalWinId()) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(internalWinId());
    SetWindowPos(
        hwnd,
        nullptr,
        nativeRect_.x(),
        nativeRect_.y(),
        nativeRect_.width(),
        nativeRect_.height(),
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#endif
}

void RegionOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect frame = rect().adjusted(1, 1, -2, -2);
    const QColor accent = recording_ ? QColor(0xe2, 0x5b, 0x4a) : QColor(0x2e, 0xe0, 0xc0);
    const QColor ink(0x16, 0x3a, 0x38);

    QPen outline(ink, 3);
    painter.setPen(outline);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame);

    QPen dash(accent, 1, Qt::DashLine);
    dash.setDashPattern({5, 4});
    painter.setPen(dash);
    painter.drawRect(frame);

    const QPoint handles[] = {
        {frame.left(), frame.top()},
        {frame.center().x(), frame.top()},
        {frame.right(), frame.top()},
        {frame.left(), frame.center().y()},
        {frame.right(), frame.center().y()},
        {frame.left(), frame.bottom()},
        {frame.center().x(), frame.bottom()},
        {frame.right(), frame.bottom()},
    };
    painter.setPen(QPen(ink, 1));
    painter.setBrush(accent);
    for (const QPoint& p : handles) {
        painter.drawRect(QRect(p.x() - kHandle / 2, p.y() - kHandle / 2, kHandle, kHandle));
    }

    const QRect native = captureRectNative();
    const QString label = QStringLiteral("%1 × %2").arg(native.width()).arg(native.height());
    QRect chip = chipRect();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawRoundedRect(chip, 4, 4);
    painter.setPen(QColor(0xf4, 0xf7, 0xf6));
    painter.drawText(chip, Qt::AlignCenter, label);
}

void RegionOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!interactive_ || event->button() != Qt::LeftButton) {
        return;
    }
    hit_ = hitTest(event->pos());
    if (hit_ == HitZone::None) {
        return;
    }
    dragging_ = true;
    dragOrigin_ = event->globalPosition().toPoint();
    dragStartGeometry_ = geometry();
}

void RegionOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!interactive_) {
        unsetCursor();
        return;
    }
    if (!dragging_) {
        updateCursor(hitTest(event->pos()));
        return;
    }
    applyDrag(event->globalPosition().toPoint());
}

void RegionOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!dragging_ || event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = false;
    hit_ = HitZone::None;
    setCaptureRect(captureRectNative());
    emit regionCommitted(captureRectNative());
}

void RegionOverlay::leaveEvent(QEvent*)
{
    if (!dragging_) {
        unsetCursor();
    }
}

bool RegionOverlay::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType != QByteArrayLiteral("windows_generic_MSG")) {
        return QWidget::nativeEvent(eventType, message, result);
    }
    const auto* msg = static_cast<MSG*>(message);
    if (msg->message != WM_NCHITTEST) {
        return QWidget::nativeEvent(eventType, message, result);
    }

    if (!interactive_) {
        *result = HTTRANSPARENT;
        return true;
    }

    RECT wr{};
    GetWindowRect(msg->hwnd, &wr);
    const qreal dpr = devicePixelRatioF();
    const int localX = qRound((GET_X_LPARAM(msg->lParam) - wr.left) / dpr);
    const int localY = qRound((GET_Y_LPARAM(msg->lParam) - wr.top) / dpr);
    const QPoint pos(localX, localY);
    if (hitTest(pos) == HitZone::None) {
        *result = HTTRANSPARENT;
        return true;
    }
    *result = HTCLIENT;
    return true;
#else
    return QWidget::nativeEvent(eventType, message, result);
#endif
}

void RegionOverlay::updateCursor(HitZone zone)
{
    switch (zone) {
    case HitZone::N:
    case HitZone::S:
        setCursor(Qt::SizeVerCursor);
        break;
    case HitZone::E:
    case HitZone::W:
        setCursor(Qt::SizeHorCursor);
        break;
    case HitZone::NE:
    case HitZone::SW:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case HitZone::NW:
    case HitZone::SE:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case HitZone::Move:
        setCursor(Qt::SizeAllCursor);
        break;
    case HitZone::None:
    default:
        unsetCursor();
        break;
    }
}

void RegionOverlay::applyDrag(const QPoint& globalPos)
{
    const QPoint delta = globalPos - dragOrigin_;
    QRect next = dragStartGeometry_;
    switch (hit_) {
    case HitZone::Move:
        next.translate(delta);
        break;
    case HitZone::N:
        next.setTop(next.top() + delta.y());
        break;
    case HitZone::S:
        next.setBottom(next.bottom() + delta.y());
        break;
    case HitZone::E:
        next.setRight(next.right() + delta.x());
        break;
    case HitZone::W:
        next.setLeft(next.left() + delta.x());
        break;
    case HitZone::NE:
        next.setTop(next.top() + delta.y());
        next.setRight(next.right() + delta.x());
        break;
    case HitZone::NW:
        next.setTop(next.top() + delta.y());
        next.setLeft(next.left() + delta.x());
        break;
    case HitZone::SE:
        next.setBottom(next.bottom() + delta.y());
        next.setRight(next.right() + delta.x());
        break;
    case HitZone::SW:
        next.setBottom(next.bottom() + delta.y());
        next.setLeft(next.left() + delta.x());
        break;
    case HitZone::None:
    default:
        return;
    }

    next = next.normalized();
    const int minW = minDipWidth();
    const int minH = minDipHeight();
    if (next.width() < minW) {
        if (hit_ == HitZone::W || hit_ == HitZone::NW || hit_ == HitZone::SW) {
            next.setLeft(next.right() - minW + 1);
        } else {
            next.setWidth(minW);
        }
    }
    if (next.height() < minH) {
        if (hit_ == HitZone::N || hit_ == HitZone::NW || hit_ == HitZone::NE) {
            next.setTop(next.bottom() - minH + 1);
        } else {
            next.setHeight(minH);
        }
    }

    const QRect desktop = dipVirtualDesktop();
    next.setWidth(qBound(minW, next.width(), desktop.width()));
    next.setHeight(qBound(minH, next.height(), desktop.height()));
    next.moveLeft(qBound(desktop.left(), next.x(), desktop.right() - next.width() + 1));
    next.moveTop(qBound(desktop.top(), next.y(), desktop.bottom() - next.height() + 1));

    if (next != geometry()) {
        applyingNative_ = true;
        setGeometry(next);
        applyingNative_ = false;
        nativeRect_ = captureRectNative();
        emitNativeChanged();
    }
}

void RegionOverlay::emitNativeChanged()
{
    update();
    emit regionChanged(captureRectNative());
}

RegionOverlay::HitZone RegionOverlay::hitTest(const QPoint& pos) const
{
    const int w = width();
    const int h = height();
    if (!rect().contains(pos)) {
        return HitZone::None;
    }

    const bool left = pos.x() < kHit;
    const bool right = pos.x() >= w - kHit;
    const bool top = pos.y() < kHit;
    const bool bottom = pos.y() >= h - kHit;

    if (top && left) {
        return HitZone::NW;
    }
    if (top && right) {
        return HitZone::NE;
    }
    if (bottom && left) {
        return HitZone::SW;
    }
    if (bottom && right) {
        return HitZone::SE;
    }
    if (top) {
        return HitZone::N;
    }
    if (bottom) {
        return HitZone::S;
    }
    if (left) {
        return HitZone::W;
    }
    if (right) {
        return HitZone::E;
    }
    if (chipRect().contains(pos)) {
        return HitZone::Move;
    }
    return HitZone::None;
}

QRect RegionOverlay::chipRect() const
{
    const int x = qBound(kChipPad, (width() - kChipW) / 2, qMax(kChipPad, width() - kChipW - kChipPad));
    return {x, kChipPad, kChipW, kChipH};
}

int RegionOverlay::minDipWidth() const
{
    return qMax(2, qRound(kMinWidth / qMax<qreal>(1.0, devicePixelRatioF())));
}

int RegionOverlay::minDipHeight() const
{
    return qMax(2, qRound(kMinHeight / qMax<qreal>(1.0, devicePixelRatioF())));
}

QRect RegionOverlay::clampNative(QRect rect)
{
    const QRect desktop = nativeVirtualDesktop();
    rect.setWidth(qBound(kMinWidth, rect.width(), desktop.width()));
    rect.setHeight(qBound(kMinHeight, rect.height(), desktop.height()));
    rect.moveLeft(qBound(desktop.left(), rect.x(), desktop.right() - rect.width() + 1));
    rect.moveTop(qBound(desktop.top(), rect.y(), desktop.bottom() - rect.height() + 1));
    return rect;
}

QRect RegionOverlay::evenSize(QRect rect)
{
    rect.setWidth(rect.width() & ~1);
    rect.setHeight(rect.height() & ~1);
    if (rect.width() < kMinWidth) {
        rect.setWidth(kMinWidth);
    }
    if (rect.height() < kMinHeight) {
        rect.setHeight(kMinHeight);
    }
    return rect;
}

RegionSelector::RegionSelector(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(dipVirtualDesktop());
}

void RegionSelector::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(0, 0, 0, 110));

    const QRect band = currentBand();
    if (band.isValid() && band.width() >= 2 && band.height() >= 2) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(band, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(0x2e, 0xe0, 0xc0), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(band.adjusted(0, 0, -1, -1));

        const QRect native = currentBandNative();
        const QString label = QStringLiteral("%1 × %2").arg(native.width()).arg(native.height());
        QRect chip(band.left() + 8, band.top() + 8, kChipW, kChipH);
        if (!rect().contains(chip)) {
            chip.moveTopLeft(band.topLeft() + QPoint(8, 8));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x16, 0x3a, 0x38));
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawRoundedRect(chip, 4, 4);
        painter.setPen(QColor(0xf4, 0xf7, 0xf6));
        painter.drawText(chip, Qt::AlignCenter, label);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QColor(0xf4, 0xf7, 0xf6, 220));
    painter.drawText(
        rect().adjusted(0, 24, 0, 0),
        Qt::AlignHCenter | Qt::AlignTop,
        tr("拖曳以選擇錄製區域，按 Esc 取消"));
}

void RegionSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    selecting_ = true;
    origin_ = event->pos();
    current_ = origin_;
    update();
}

void RegionSelector::mouseMoveEvent(QMouseEvent* event)
{
    if (!selecting_) {
        return;
    }
    current_ = event->pos();
    update();
}

void RegionSelector::mouseReleaseEvent(QMouseEvent* event)
{
    if (!selecting_ || event->button() != Qt::LeftButton) {
        return;
    }
    selecting_ = false;
    const QRect native = currentBandNative();
    if (native.width() < RegionOverlay::kMinWidth || native.height() < RegionOverlay::kMinHeight) {
        emit cancelled();
        close();
        return;
    }
    emit selected(native);
    close();
}

void RegionSelector::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

QRect RegionSelector::currentBand() const
{
    if (!selecting_ && origin_ == current_) {
        return {};
    }
    return normalized(origin_, current_);
}

QRect RegionSelector::currentBandNative() const
{
    const QRect band = currentBand();
    if (!band.isValid()) {
        return {};
    }
    const QRect dip(mapToGlobal(band.topLeft()), band.size());
    return dipToNativeRect(dip, screenForDip(dip.center()));
}

} // namespace ors
