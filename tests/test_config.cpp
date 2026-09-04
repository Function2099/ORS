#include "core/Config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Config round-trip preserves schema fields")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.json"));

    ors::Config config;
    config.data().language = QStringLiteral("en_US");
    config.data().lastTab = QStringLiteral("game");
    config.data().directory = QStringLiteral("D:/Videos/ORS");
    config.data().filenameTemplate = QStringLiteral("TEST_yyyyMMdd_HHmmss");
    config.data().regionPreset = QStringLiteral("youtube-1080");
    config.data().customWidth = 1280;
    config.data().customHeight = 720;
    config.data().savedRegions.push_back({QStringLiteral("Stream"), 1920, 1080});
    config.data().container = QStringLiteral("wmv");
    config.data().video = QStringLiteral("wmv");
    config.data().audio = QStringLiteral("wma");
    config.data().systemAudio = false;
    config.data().microphoneId = QStringLiteral("mic-1");
    config.data().toggleRecord = QStringLiteral("F8");
    config.data().togglePause = QStringLiteral("F7");

    REQUIRE(config.save(path));
    REQUIRE(QFile::exists(path));

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE(loaded.data().version == 1);
    REQUIRE(loaded.data().language == QStringLiteral("en_US"));
    REQUIRE(loaded.data().lastTab == QStringLiteral("game"));
    REQUIRE(loaded.data().directory == QStringLiteral("D:/Videos/ORS"));
    REQUIRE(loaded.data().filenameTemplate == QStringLiteral("TEST_yyyyMMdd_HHmmss"));
    REQUIRE(loaded.data().regionPreset == QStringLiteral("youtube-1080"));
    REQUIRE(loaded.data().customWidth == 1280);
    REQUIRE(loaded.data().customHeight == 720);
    REQUIRE(loaded.data().savedRegions.size() == 1);
    REQUIRE(loaded.data().savedRegions.front().name == QStringLiteral("Stream"));
    REQUIRE(loaded.data().savedRegions.front().width == 1920);
    REQUIRE(loaded.data().container == QStringLiteral("wmv"));
    REQUIRE(loaded.data().systemAudio == false);
    REQUIRE(loaded.data().microphoneId == QStringLiteral("mic-1"));
    REQUIRE(loaded.data().toggleRecord == QStringLiteral("F8"));
}

TEST_CASE("Config missing file loads defaults")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("missing.json"));

    ors::Config config;
    REQUIRE(config.load(path));
    REQUIRE(config.data().version == 1);
    REQUIRE(config.data().language == QStringLiteral("zh_TW"));
    REQUIRE(config.data().lastTab == QStringLiteral("screen"));
    REQUIRE(config.data().filenameTemplate == QStringLiteral("ORS_yyyyMMdd_HHmmss"));
    REQUIRE(config.data().container == QStringLiteral("mp4"));
}

TEST_CASE("Config JSON uses nested schema keys")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("schema.json"));

    ors::Config config;
    REQUIRE(config.save(path));

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    REQUIRE(root.contains(QStringLiteral("version")));
    REQUIRE(root.contains(QStringLiteral("ui")));
    REQUIRE(root.contains(QStringLiteral("output")));
    REQUIRE(root.contains(QStringLiteral("region")));
    REQUIRE(root.contains(QStringLiteral("codec")));
    REQUIRE(root.contains(QStringLiteral("audio")));
    REQUIRE(root.contains(QStringLiteral("hotkeys")));
}

TEST_CASE("Empty output directory resolves to movies or home")
{
    ors::ConfigData data;
    const QString resolved = ors::Config::resolvedOutputDirectory(data);
    REQUIRE_FALSE(resolved.isEmpty());
}
