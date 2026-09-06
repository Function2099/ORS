#pragma once

#include <QColor>
#include <QIcon>

namespace ors {

enum class ToolbarGlyph {
    Record,
    Stop,
    Pause,
    Resume,
    Capture,
    Region,
    Open,
    Codec,
    Sound,
    Settings,
};

QIcon toolbarIcon(ToolbarGlyph glyph, int logicalSize = 22, const QColor& color = QColor(0x1f, 0x4e, 0x4c));

} // namespace ors
