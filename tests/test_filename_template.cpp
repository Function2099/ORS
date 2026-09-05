#include "core/FilenameTemplate.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTime>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Bandicam-style template expands prefix and timestamp")
{
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString name = ors::expandFilenameTemplate(
        QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"),
        QStringLiteral("錄製"),
        when,
        1);
    REQUIRE(name == QStringLiteral("錄製_2026_09_05_16_53_48_163"));
}

TEST_CASE("Qt date format templates still expand")
{
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString name = ors::expandFilenameTemplate(
        QStringLiteral("ORS_yyyyMMdd_HHmmss"),
        QStringLiteral("錄製"),
        when,
        1);
    REQUIRE(name == QStringLiteral("ORS_20260905_165348"));
}

TEST_CASE("unbracketed hashes pad the starting number")
{
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    REQUIRE(
        ors::expandFilenameTemplate(QStringLiteral("<Prefix>_#"), QStringLiteral("錄製"), when, 7)
        == QStringLiteral("錄製_7"));
    REQUIRE(
        ors::expandFilenameTemplate(QStringLiteral("<Prefix>_##"), QStringLiteral("錄製"), when, 7)
        == QStringLiteral("錄製_07"));
    REQUIRE(
        ors::expandFilenameTemplate(QStringLiteral("<Prefix>_###"), QStringLiteral("錄製"), when, 7)
        == QStringLiteral("錄製_007"));
}

TEST_CASE("name and user tokens expand from context")
{
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    ors::FilenameContext context;
    context.prefix = QStringLiteral("錄製");
    context.appName = QStringLiteral("ORS");
    context.userId = QStringLiteral("alice");
    context.displayName = QStringLiteral("Alice Chen");
    REQUIRE(
        ors::expandFilenameTemplate(
            QStringLiteral("<Name>_<User>_<DisplayName>_#"), context, when, 3)
        == QStringLiteral("ORS_alice_Alice Chen_3"));
}

TEST_CASE("localized date token omits angle brackets")
{
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString name = ors::expandFilenameTemplate(
        QStringLiteral("<Prefix>_<Date>_#"),
        QStringLiteral("錄製"),
        when,
        1);
    REQUIRE(name.startsWith(QStringLiteral("錄製_")));
    REQUIRE(name.endsWith(QStringLiteral("_1")));
    REQUIRE_FALSE(name.contains(QLatin1Char('<')));
    REQUIRE(name.contains(QStringLiteral("2026")));
}

TEST_CASE("unique numbered template increments the token")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString first = dir.filePath(QStringLiteral("錄製_1.mp4"));
    {
        QFile file(first);
        REQUIRE(file.open(QIODevice::WriteOnly));
    }
    const QString path = ors::makeUniqueOutputPath(
        dir.path(),
        QStringLiteral("<Prefix>_#"),
        QStringLiteral("錄製"),
        1,
        QStringLiteral(".mp4"),
        when);
    REQUIRE(QFileInfo(path).fileName() == QStringLiteral("錄製_2.mp4"));
}

TEST_CASE("unique path uses template name when free")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString path = ors::makeUniqueOutputPath(
        dir.path(),
        QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"),
        QStringLiteral("錄製"),
        1,
        QStringLiteral(".mp4"),
        when);
    REQUIRE(QFileInfo(path).fileName() == QStringLiteral("錄製_2026_09_05_16_53_48_163.mp4"));
}

TEST_CASE("unique path appends number on collision")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDateTime when(QDate(2026, 9, 5), QTime(16, 53, 48, 163));
    const QString first = dir.filePath(QStringLiteral("錄製_2026_09_05_16_53_48_163.mp4"));
    {
        QFile file(first);
        REQUIRE(file.open(QIODevice::WriteOnly));
    }
    const QString path = ors::makeUniqueOutputPath(
        dir.path(),
        QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"),
        QStringLiteral("錄製"),
        1,
        QStringLiteral(".mp4"),
        when);
    REQUIRE(QFileInfo(path).fileName() == QStringLiteral("錄製_2026_09_05_16_53_48_163_1.mp4"));
}
