#include "ui/ToolbarIcons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace ors {
namespace {

qreal devicePixelRatio()
{
    if (const QGuiApplication* app = qGuiApp) {
        return app->devicePixelRatio();
    }
    return 1.0;
}

QPen stroke(const QColor& color, qreal width = 1.8)
{
    QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    return pen;
}

void paintRecord(QPainter& p, const QRectF& box, const QColor& color)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(box.adjusted(5.5, 5.5, -5.5, -5.5));
}

void paintStop(QPainter& p, const QRectF& box, const QColor& color)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(box.adjusted(6.2, 6.2, -6.2, -6.2), 2.2, 2.2);
}

void paintCapture(QPainter& p, const QRectF& box, const QColor& color)
{
    const QRectF frame = box.adjusted(3.5, 4.5, -3.5, -4.5);
    p.setPen(stroke(color));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(frame, 2.5, 2.5);
    p.drawLine(QPointF(frame.left() + 3, frame.center().y()),
               QPointF(frame.right() - 3, frame.center().y()));
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(frame.right() - 5.5, frame.top() + 5.5), 1.6, 1.6);
}

void paintRegion(QPainter& p, const QRectF& box, const QColor& color)
{
    const QRectF frame = box.adjusted(4, 4.5, -4, -4.5);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 1.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(frame, 2, 2);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const qreal h = 3.2;
    p.drawRect(QRectF(frame.left() - 0.5, frame.top() - 0.5, h, h));
    p.drawRect(QRectF(frame.right() - h + 0.5, frame.top() - 0.5, h, h));
    p.drawRect(QRectF(frame.left() - 0.5, frame.bottom() - h + 0.5, h, h));
    p.drawRect(QRectF(frame.right() - h + 0.5, frame.bottom() - h + 0.5, h, h));
}

void paintOpen(QPainter& p, const QRectF& box, const QColor& color)
{
    QPainterPath path;
    path.moveTo(box.left() + 4, box.top() + 9);
    path.lineTo(box.left() + 4, box.top() + 7.2);
    path.quadTo(box.left() + 4, box.top() + 5.5, box.left() + 5.6, box.top() + 5.5);
    path.lineTo(box.left() + 10.2, box.top() + 5.5);
    path.lineTo(box.left() + 12.2, box.top() + 8);
    path.lineTo(box.right() - 4, box.top() + 8);
    path.quadTo(box.right() - 2.6, box.top() + 8, box.right() - 2.6, box.top() + 9.4);
    path.lineTo(box.right() - 2.6, box.bottom() - 5);
    path.quadTo(box.right() - 2.6, box.bottom() - 3.6, box.right() - 4, box.bottom() - 3.6);
    path.lineTo(box.left() + 4, box.bottom() - 3.6);
    path.quadTo(box.left() + 2.6, box.bottom() - 3.6, box.left() + 2.6, box.bottom() - 5);
    path.lineTo(box.left() + 2.6, box.top() + 9);
    path.closeSubpath();
    p.setPen(stroke(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void paintCodec(QPainter& p, const QRectF& box, const QColor& color)
{
    const QRectF back = box.adjusted(6.5, 4, -3.5, -7);
    const QRectF front = box.adjusted(3.5, 7, -6.5, -4);
    p.setPen(stroke(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(back, 2, 2);
    p.drawRoundedRect(front, 2, 2);
}

void paintSound(QPainter& p, const QRectF& box, const QColor& color)
{
    const QRectF capsule(box.center().x() - 3.2, box.top() + 4.2, 6.4, 9.5);
    p.setPen(stroke(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(capsule, 3.2, 3.2);

    QPainterPath yoke;
    const QRectF arc(capsule.left() - 3.4, capsule.top() + 3.2, capsule.width() + 6.8, 10.5);
    yoke.arcMoveTo(arc, 200);
    yoke.arcTo(arc, 200, 140);
    p.drawPath(yoke);
    p.drawLine(QPointF(box.center().x(), yoke.boundingRect().bottom()),
               QPointF(box.center().x(), box.bottom() - 5.2));
    p.drawLine(QPointF(box.center().x() - 4.2, box.bottom() - 5.2),
               QPointF(box.center().x() + 4.2, box.bottom() - 5.2));
}

void paintSettings(QPainter& p, const QRectF& box, const QColor& color)
{
    const QPointF c = box.center();
    p.save();
    p.translate(c);
    p.setPen(stroke(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(0, 0), 3.6, 3.6);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    for (int i = 0; i < 6; ++i) {
        p.save();
        p.rotate(i * 60.0);
        p.drawRoundedRect(QRectF(-1.05, -7.4, 2.1, 2.8), 0.6, 0.6);
        p.restore();
    }
    p.restore();
}

} // namespace

QIcon toolbarIcon(ToolbarGlyph glyph, int logicalSize, const QColor& color)
{
    const qreal dpr = devicePixelRatio();
    const int px = qMax(1, qRound(logicalSize * dpr));
    QPixmap pixmap(px, px);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box(0, 0, logicalSize, logicalSize);
    switch (glyph) {
    case ToolbarGlyph::Record:
        paintRecord(painter, box, color);
        break;
    case ToolbarGlyph::Stop:
        paintStop(painter, box, color);
        break;
    case ToolbarGlyph::Capture:
        paintCapture(painter, box, color);
        break;
    case ToolbarGlyph::Region:
        paintRegion(painter, box, color);
        break;
    case ToolbarGlyph::Open:
        paintOpen(painter, box, color);
        break;
    case ToolbarGlyph::Codec:
        paintCodec(painter, box, color);
        break;
    case ToolbarGlyph::Sound:
        paintSound(painter, box, color);
        break;
    case ToolbarGlyph::Settings:
        paintSettings(painter, box, color);
        break;
    }
    painter.end();
    return QIcon(pixmap);
}

} // namespace ors
