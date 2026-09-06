#pragma once

#include <Qt>
#include <QString>
#include <QVector>

#include <functional>

namespace ors {

struct ConfigData;

struct HotkeySpec {
    Qt::KeyboardModifiers modifiers{};
    Qt::Key key{Qt::Key_unknown};

    bool isValid() const { return key != Qt::Key_unknown; }
};

bool parseHotkey(const QString& text, HotkeySpec* spec);
QString hotkeyToPortableString(const HotkeySpec& spec);
QString normalizeHotkey(const QString& text);

enum class HotkeyAction {
    ToggleRecord = 0xB101,
    TogglePause = 0xB102,
    CaptureStill = 0xB103,
    SelectTarget = 0xB104,
};

QString hotkeySequenceFor(const ConfigData& data, HotkeyAction action);

class HotkeyRegistrar {
public:
    using Handler = std::function<void(HotkeyAction)>;

    HotkeyRegistrar() = default;
    ~HotkeyRegistrar();

    HotkeyRegistrar(const HotkeyRegistrar&) = delete;
    HotkeyRegistrar& operator=(const HotkeyRegistrar&) = delete;

    void setHandler(Handler handler);
    void unregisterAll();
    QVector<HotkeyAction> registerFrom(ConfigData& data);
    void dispatch(int id);

private:
    bool ensureSinkWindow();
    void destroySinkWindow();

    Handler handler_;
    void* hwnd_{};
    QVector<int> ids_;
};

} // namespace ors
