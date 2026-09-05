#pragma once

#include <QRect>
#include <QWidget>

class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QScreen;
class QShowEvent;

namespace ors {

QRect nativeVirtualDesktop();
QRect nativeScreenGeometry(QScreen* screen);

class RegionOverlay : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMinWidth = 64;
    static constexpr int kMinHeight = 64;

    explicit RegionOverlay(QWidget* parent = nullptr);

    QRect captureRect() const;
    QRect captureRectNative() const;
    void setCaptureRect(const QRect& nativeRect);

    void setInteractive(bool enabled);
    bool isInteractive() const { return interactive_; }

    void setRecording(bool recording);

signals:
    void regionChanged(const QRect& nativeRect);
    void regionCommitted(const QRect& nativeRect);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    enum class HitZone {
        None,
        Move,
        N,
        S,
        E,
        W,
        NE,
        NW,
        SE,
        SW,
    };

    void applyNativeWindowHints();
    void applyWin32Bounds();
    void updateCursor(HitZone zone);
    void applyDrag(const QPoint& globalPos);
    void emitNativeChanged();
    HitZone hitTest(const QPoint& pos) const;
    QRect chipRect() const;
    int minDipWidth() const;
    int minDipHeight() const;
    static QRect clampNative(QRect rect);
    static QRect evenSize(QRect rect);

    bool interactive_{true};
    bool recording_{false};
    bool dragging_{false};
    bool applyingNative_{false};
    HitZone hit_{HitZone::None};
    QPoint dragOrigin_;
    QRect dragStartGeometry_;
    QRect nativeRect_{};
};

class RegionSelector : public QWidget {
    Q_OBJECT

public:
    explicit RegionSelector(QWidget* parent = nullptr);

signals:
    void selected(const QRect& nativeRect);
    void cancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QRect currentBand() const;
    QRect currentBandNative() const;

    bool selecting_{false};
    QPoint origin_;
    QPoint current_;
};

} // namespace ors
