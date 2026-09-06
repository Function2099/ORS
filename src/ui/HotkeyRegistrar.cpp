#include "ui/HotkeyRegistrar.h"

#include "core/Config.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <QHash>
#include <utility>

namespace ors {
namespace {

bool isModifierToken(const QString& token)
{
    return token == QLatin1String("SHIFT") || token == QLatin1String("CTRL")
        || token == QLatin1String("CONTROL") || token == QLatin1String("ALT")
        || token == QLatin1String("META") || token == QLatin1String("WIN")
        || token == QLatin1String("WINDOWS");
}

Qt::Key keyFromName(const QString& token)
{
    static const QHash<QString, Qt::Key> named{
        {QStringLiteral("SPACE"), Qt::Key_Space},
        {QStringLiteral("TAB"), Qt::Key_Tab},
        {QStringLiteral("ESC"), Qt::Key_Escape},
        {QStringLiteral("ESCAPE"), Qt::Key_Escape},
        {QStringLiteral("BACKSPACE"), Qt::Key_Backspace},
        {QStringLiteral("BACK"), Qt::Key_Backspace},
        {QStringLiteral("INSERT"), Qt::Key_Insert},
        {QStringLiteral("INS"), Qt::Key_Insert},
        {QStringLiteral("DELETE"), Qt::Key_Delete},
        {QStringLiteral("DEL"), Qt::Key_Delete},
        {QStringLiteral("HOME"), Qt::Key_Home},
        {QStringLiteral("END"), Qt::Key_End},
        {QStringLiteral("PGUP"), Qt::Key_PageUp},
        {QStringLiteral("PAGEUP"), Qt::Key_PageUp},
        {QStringLiteral("PGDN"), Qt::Key_PageDown},
        {QStringLiteral("PAGEDOWN"), Qt::Key_PageDown},
        {QStringLiteral("LEFT"), Qt::Key_Left},
        {QStringLiteral("RIGHT"), Qt::Key_Right},
        {QStringLiteral("UP"), Qt::Key_Up},
        {QStringLiteral("DOWN"), Qt::Key_Down},
        {QStringLiteral("PRINT"), Qt::Key_Print},
        {QStringLiteral("PAUSE"), Qt::Key_Pause},
        {QStringLiteral("PLUS"), Qt::Key_Plus},
        {QStringLiteral("MINUS"), Qt::Key_Minus},
        {QStringLiteral("COMMA"), Qt::Key_Comma},
        {QStringLiteral("PERIOD"), Qt::Key_Period},
    };
    if (const auto it = named.constFind(token); it != named.cend()) {
        return it.value();
    }
    if (token.size() >= 2 && token.startsWith(QLatin1Char('F'))) {
        bool ok = false;
        const int index = token.mid(1).toInt(&ok);
        if (ok && index >= 1 && index <= 24) {
            return static_cast<Qt::Key>(Qt::Key_F1 + (index - 1));
        }
    }
    if (token.size() == 1) {
        const QChar ch = token.at(0);
        if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
            return static_cast<Qt::Key>(Qt::Key_A + (ch.unicode() - 'A'));
        }
        if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
            return static_cast<Qt::Key>(Qt::Key_0 + (ch.unicode() - '0'));
        }
    }
    return Qt::Key_unknown;
}

QString nameFromKey(Qt::Key key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return QStringLiteral("F%1").arg(1 + static_cast<int>(key - Qt::Key_F1));
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QChar(QLatin1Char('A' + static_cast<int>(key - Qt::Key_A)));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QChar(QLatin1Char('0' + static_cast<int>(key - Qt::Key_0)));
    }
    switch (key) {
    case Qt::Key_Space:
        return QStringLiteral("Space");
    case Qt::Key_Tab:
        return QStringLiteral("Tab");
    case Qt::Key_Escape:
        return QStringLiteral("Esc");
    case Qt::Key_Backspace:
        return QStringLiteral("Backspace");
    case Qt::Key_Insert:
        return QStringLiteral("Insert");
    case Qt::Key_Delete:
        return QStringLiteral("Delete");
    case Qt::Key_Home:
        return QStringLiteral("Home");
    case Qt::Key_End:
        return QStringLiteral("End");
    case Qt::Key_PageUp:
        return QStringLiteral("PgUp");
    case Qt::Key_PageDown:
        return QStringLiteral("PgDown");
    case Qt::Key_Left:
        return QStringLiteral("Left");
    case Qt::Key_Right:
        return QStringLiteral("Right");
    case Qt::Key_Up:
        return QStringLiteral("Up");
    case Qt::Key_Down:
        return QStringLiteral("Down");
    case Qt::Key_Print:
        return QStringLiteral("Print");
    case Qt::Key_Pause:
        return QStringLiteral("Pause");
    case Qt::Key_Plus:
        return QStringLiteral("Plus");
    case Qt::Key_Minus:
        return QStringLiteral("Minus");
    case Qt::Key_Comma:
        return QStringLiteral("Comma");
    case Qt::Key_Period:
        return QStringLiteral("Period");
    default:
        return {};
    }
}

#ifdef Q_OS_WIN
bool toNative(const HotkeySpec& spec, UINT* modifiers, UINT* vk)
{
    UINT nativeMods = 0;
    if (spec.modifiers & Qt::ShiftModifier) {
        nativeMods |= MOD_SHIFT;
    }
    if (spec.modifiers & Qt::ControlModifier) {
        nativeMods |= MOD_CONTROL;
    }
    if (spec.modifiers & Qt::AltModifier) {
        nativeMods |= MOD_ALT;
    }
    if (spec.modifiers & Qt::MetaModifier) {
        nativeMods |= MOD_WIN;
    }

    UINT nativeVk = 0;
    if (spec.key >= Qt::Key_F1 && spec.key <= Qt::Key_F24) {
        nativeVk = VK_F1 + static_cast<UINT>(spec.key - Qt::Key_F1);
    } else if (spec.key >= Qt::Key_A && spec.key <= Qt::Key_Z) {
        nativeVk = static_cast<UINT>('A' + (spec.key - Qt::Key_A));
    } else if (spec.key >= Qt::Key_0 && spec.key <= Qt::Key_9) {
        nativeVk = static_cast<UINT>('0' + (spec.key - Qt::Key_0));
    } else {
        switch (spec.key) {
        case Qt::Key_Space:
            nativeVk = VK_SPACE;
            break;
        case Qt::Key_Tab:
            nativeVk = VK_TAB;
            break;
        case Qt::Key_Escape:
            nativeVk = VK_ESCAPE;
            break;
        case Qt::Key_Backspace:
            nativeVk = VK_BACK;
            break;
        case Qt::Key_Insert:
            nativeVk = VK_INSERT;
            break;
        case Qt::Key_Delete:
            nativeVk = VK_DELETE;
            break;
        case Qt::Key_Home:
            nativeVk = VK_HOME;
            break;
        case Qt::Key_End:
            nativeVk = VK_END;
            break;
        case Qt::Key_PageUp:
            nativeVk = VK_PRIOR;
            break;
        case Qt::Key_PageDown:
            nativeVk = VK_NEXT;
            break;
        case Qt::Key_Left:
            nativeVk = VK_LEFT;
            break;
        case Qt::Key_Right:
            nativeVk = VK_RIGHT;
            break;
        case Qt::Key_Up:
            nativeVk = VK_UP;
            break;
        case Qt::Key_Down:
            nativeVk = VK_DOWN;
            break;
        case Qt::Key_Print:
            nativeVk = VK_SNAPSHOT;
            break;
        case Qt::Key_Pause:
            nativeVk = VK_PAUSE;
            break;
        case Qt::Key_Plus:
            nativeVk = VK_OEM_PLUS;
            break;
        case Qt::Key_Minus:
            nativeVk = VK_OEM_MINUS;
            break;
        case Qt::Key_Comma:
            nativeVk = VK_OEM_COMMA;
            break;
        case Qt::Key_Period:
            nativeVk = VK_OEM_PERIOD;
            break;
        default:
            return false;
        }
    }

    if (nativeVk == 0) {
        return false;
    }
    *modifiers = nativeMods;
    *vk = nativeVk;
    return true;
}
#endif

} // namespace

bool parseHotkey(const QString& text, HotkeySpec* spec)
{
    HotkeySpec parsed;
    const QStringList parts = text.trimmed().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    for (int i = 0; i < parts.size(); ++i) {
        const QString token = parts.at(i).trimmed().toUpper();
        if (token.isEmpty()) {
            return false;
        }
        const bool last = (i == parts.size() - 1);
        if (isModifierToken(token)) {
            if (token == QLatin1String("SHIFT")) {
                parsed.modifiers |= Qt::ShiftModifier;
            } else if (token == QLatin1String("CTRL") || token == QLatin1String("CONTROL")) {
                parsed.modifiers |= Qt::ControlModifier;
            } else if (token == QLatin1String("ALT")) {
                parsed.modifiers |= Qt::AltModifier;
            } else {
                parsed.modifiers |= Qt::MetaModifier;
            }
            if (last) {
                return false;
            }
            continue;
        }
        if (!last) {
            return false;
        }
        parsed.key = keyFromName(token);
    }
    if (!parsed.isValid()) {
        return false;
    }
    if (spec) {
        *spec = parsed;
    }
    return true;
}

QString hotkeyToPortableString(const HotkeySpec& spec)
{
    if (!spec.isValid()) {
        return {};
    }
    const QString key = nameFromKey(spec.key);
    if (key.isEmpty()) {
        return {};
    }
    QStringList parts;
    if (spec.modifiers & Qt::ControlModifier) {
        parts.append(QStringLiteral("Ctrl"));
    }
    if (spec.modifiers & Qt::AltModifier) {
        parts.append(QStringLiteral("Alt"));
    }
    if (spec.modifiers & Qt::ShiftModifier) {
        parts.append(QStringLiteral("Shift"));
    }
    if (spec.modifiers & Qt::MetaModifier) {
        parts.append(QStringLiteral("Meta"));
    }
    parts.append(key);
    return parts.join(QLatin1Char('+'));
}

QString normalizeHotkey(const QString& text)
{
    HotkeySpec spec;
    if (!parseHotkey(text, &spec)) {
        return {};
    }
    return hotkeyToPortableString(spec);
}

QString hotkeySequenceFor(const ConfigData& data, HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::ToggleRecord:
        return data.toggleRecord;
    case HotkeyAction::TogglePause:
        return data.togglePause;
    case HotkeyAction::CaptureStill:
        return data.captureStill;
    case HotkeyAction::SelectTarget:
        return data.selectTarget;
    }
    return {};
}

#ifdef Q_OS_WIN
LRESULT CALLBACK orsHotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<HotkeyRegistrar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const auto* created = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<HotkeyRegistrar*>(created->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && msg == WM_HOTKEY) {
        self->dispatch(static_cast<int>(wParam));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

void HotkeyRegistrar::setHandler(Handler handler)
{
    handler_ = std::move(handler);
}

void HotkeyRegistrar::dispatch(int id)
{
    if (handler_) {
        handler_(static_cast<HotkeyAction>(id));
    }
}

HotkeyRegistrar::~HotkeyRegistrar()
{
    unregisterAll();
    destroySinkWindow();
}

void HotkeyRegistrar::unregisterAll()
{
#ifdef Q_OS_WIN
    if (hwnd_) {
        for (int id : ids_) {
            UnregisterHotKey(static_cast<HWND>(hwnd_), id);
        }
    }
#endif
    ids_.clear();
}

void HotkeyRegistrar::destroySinkWindow()
{
#ifdef Q_OS_WIN
    if (hwnd_) {
        DestroyWindow(static_cast<HWND>(hwnd_));
        hwnd_ = nullptr;
    }
#else
    hwnd_ = nullptr;
#endif
}

bool HotkeyRegistrar::ensureSinkWindow()
{
#ifdef Q_OS_WIN
    if (hwnd_) {
        return true;
    }
    static ATOM atom = 0;
    if (atom == 0) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = orsHotkeyWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"ORSHotkeySink";
        atom = RegisterClassW(&wc);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        if (atom == 0) {
            atom = 1;
        }
    }
    HWND hwnd = CreateWindowExW(
        0,
        L"ORSHotkeySink",
        L"",
        0,
        0,
        0,
        0,
        0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandleW(nullptr),
        this);
    if (!hwnd) {
        return false;
    }
    hwnd_ = hwnd;
    return true;
#else
    return false;
#endif
}

QVector<HotkeyAction> HotkeyRegistrar::registerFrom(ConfigData& data)
{
    unregisterAll();
    QVector<HotkeyAction> failed;
#ifndef Q_OS_WIN
    Q_UNUSED(data);
    return failed;
#else
    if (!ensureSinkWindow()) {
        failed.append(HotkeyAction::ToggleRecord);
        failed.append(HotkeyAction::TogglePause);
        failed.append(HotkeyAction::CaptureStill);
        failed.append(HotkeyAction::SelectTarget);
        return failed;
    }

    const auto tryRegister = [this, &failed](HotkeyAction action, bool enabled, QString& sequence) {
        if (!enabled) {
            return;
        }
        HotkeySpec spec;
        UINT mods = 0;
        UINT vk = 0;
        const int id = static_cast<int>(action);
        if (!parseHotkey(sequence, &spec) || !toNative(spec, &mods, &vk)) {
            failed.append(action);
            return;
        }
        const UINT flags = mods | MOD_NOREPEAT;
        if (RegisterHotKey(static_cast<HWND>(hwnd_), id, flags, vk)) {
            ids_.append(id);
            return;
        }
        if (spec.modifiers == Qt::NoModifier) {
            const UINT ctrlFlags = flags | MOD_CONTROL;
            if (RegisterHotKey(static_cast<HWND>(hwnd_), id, ctrlFlags, vk)) {
                spec.modifiers = Qt::ControlModifier;
                sequence = hotkeyToPortableString(spec);
                ids_.append(id);
                return;
            }
        }
        failed.append(action);
    };

    tryRegister(HotkeyAction::ToggleRecord, data.toggleRecordEnabled, data.toggleRecord);
    tryRegister(HotkeyAction::TogglePause, data.togglePauseEnabled, data.togglePause);
    tryRegister(HotkeyAction::CaptureStill, data.captureStillEnabled, data.captureStill);
    tryRegister(HotkeyAction::SelectTarget, data.selectTargetEnabled, data.selectTarget);
    return failed;
#endif
}

} // namespace ors
