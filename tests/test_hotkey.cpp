#include "ui/HotkeyRegistrar.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parseHotkey accepts function keys and modifiers")
{
    ors::HotkeySpec spec;
    REQUIRE(ors::parseHotkey(QStringLiteral("F9"), &spec));
    REQUIRE(spec.key == Qt::Key_F9);
    REQUIRE(spec.modifiers == Qt::NoModifier);
    REQUIRE(ors::hotkeyToPortableString(spec) == QStringLiteral("F9"));

    REQUIRE(ors::parseHotkey(QStringLiteral("Shift+F2"), &spec));
    REQUIRE(spec.key == Qt::Key_F2);
    REQUIRE(spec.modifiers == Qt::ShiftModifier);
    REQUIRE(ors::hotkeyToPortableString(spec) == QStringLiteral("Shift+F2"));

    REQUIRE(ors::parseHotkey(QStringLiteral("ctrl+alt+f3"), &spec));
    REQUIRE(spec.key == Qt::Key_F3);
    REQUIRE(spec.modifiers == (Qt::ControlModifier | Qt::AltModifier));
    REQUIRE(ors::hotkeyToPortableString(spec) == QStringLiteral("Ctrl+Alt+F3"));
}

TEST_CASE("parseHotkey rejects empty modifier-only and unknown tokens")
{
    ors::HotkeySpec spec;
    REQUIRE_FALSE(ors::parseHotkey(QString(), &spec));
    REQUIRE_FALSE(ors::parseHotkey(QStringLiteral("Shift"), &spec));
    REQUIRE_FALSE(ors::parseHotkey(QStringLiteral("Ctrl+"), &spec));
    REQUIRE_FALSE(ors::parseHotkey(QStringLiteral("Foo"), &spec));
    REQUIRE_FALSE(ors::parseHotkey(QStringLiteral("F25"), &spec));
}

TEST_CASE("normalizeHotkey canonicalizes portable text")
{
    REQUIRE(ors::normalizeHotkey(QStringLiteral("f9")) == QStringLiteral("F9"));
    REQUIRE(ors::normalizeHotkey(QStringLiteral("SHIFT+F2")) == QStringLiteral("Shift+F2"));
    REQUIRE(ors::normalizeHotkey(QStringLiteral("Control+A")) == QStringLiteral("Ctrl+A"));
    REQUIRE(ors::normalizeHotkey(QStringLiteral("not-a-key")).isEmpty());
}
