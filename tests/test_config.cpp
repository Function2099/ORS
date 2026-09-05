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
    config.data().filenameTemplate = QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>");
    config.data().filenamePrefix = QStringLiteral("錄製");
    config.data().filenameStartNumber = 3;
    config.data().regionPreset = QStringLiteral("1080p");
    config.data().hasCustomPosition = true;
    config.data().customX = 120;
    config.data().customY = 80;
    config.data().customWidth = 1280;
    config.data().customHeight = 720;
    config.data().savedRegions.push_back({QStringLiteral("Stream"), 1920, 1080});
    config.data().container = QStringLiteral("wmv");
    config.data().video = QStringLiteral("wmv");
    config.data().audio = QStringLiteral("wma");
    config.data().systemAudio = false;
    config.data().microphoneId = QStringLiteral("mic-1");
    config.data().microphoneInputSource = QStringLiteral("left");
    config.data().toggleRecord = QStringLiteral("F8");
    config.data().togglePause = QStringLiteral("F7");
    config.data().includeCursor = false;
    config.data().alwaysOnTop = true;
    config.data().useTrayIcon = false;
    config.data().frameRate = 60;
    config.data().videoQuality = QStringLiteral("medium");
    config.data().keyframeInterval = 8;
    config.data().resolutionAlign = QStringLiteral("16x16");
    config.data().frameRateMode = QStringLiteral("cfr");
    config.data().captureIncludeCursor = false;
    config.data().captureImageFormat = QStringLiteral("jpg");

    REQUIRE(config.save(path));
    REQUIRE(QFile::exists(path));

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE(loaded.data().version == 1);
    REQUIRE(loaded.data().language == QStringLiteral("en_US"));
    REQUIRE(loaded.data().lastTab == QStringLiteral("game"));
    REQUIRE(loaded.data().directory == QStringLiteral("D:/Videos/ORS"));
    REQUIRE(loaded.data().filenameTemplate == QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"));
    REQUIRE(loaded.data().filenamePrefix == QStringLiteral("錄製"));
    REQUIRE(loaded.data().filenameStartNumber == 3);
    REQUIRE(loaded.data().regionPreset == QStringLiteral("1080p"));
    REQUIRE(loaded.data().hasCustomPosition);
    REQUIRE(loaded.data().customX == 120);
    REQUIRE(loaded.data().customY == 80);
    REQUIRE(loaded.data().customWidth == 1280);
    REQUIRE(loaded.data().customHeight == 720);
    REQUIRE(loaded.data().savedRegions.size() == 1);
    REQUIRE(loaded.data().savedRegions.front().name == QStringLiteral("Stream"));
    REQUIRE(loaded.data().savedRegions.front().width == 1920);
    REQUIRE(loaded.data().container == QStringLiteral("wmv"));
    REQUIRE(loaded.data().systemAudio == false);
    REQUIRE(loaded.data().microphoneId == QStringLiteral("mic-1"));
    REQUIRE(loaded.data().microphoneInputSource == QStringLiteral("left"));
    REQUIRE(loaded.data().toggleRecord == QStringLiteral("F8"));
    REQUIRE_FALSE(loaded.data().includeCursor);
    REQUIRE(loaded.data().alwaysOnTop);
    REQUIRE_FALSE(loaded.data().useTrayIcon);
    REQUIRE(loaded.data().frameRate == 60);
    REQUIRE(loaded.data().videoQuality == QStringLiteral("medium"));
    REQUIRE(loaded.data().keyframeInterval == 8);
    REQUIRE(loaded.data().resolutionAlign == QStringLiteral("16x16"));
    REQUIRE(loaded.data().frameRateMode == QStringLiteral("cfr"));
    REQUIRE_FALSE(loaded.data().captureIncludeCursor);
    REQUIRE(loaded.data().captureImageFormat == QStringLiteral("jpg"));
}

TEST_CASE("gopFrameCount is keyframe seconds times fps")
{
    ors::ConfigData data;
    data.frameRate = 30;
    data.keyframeInterval = 5;
    REQUIRE(ors::Config::gopFrameCount(data) == 150);
    data.frameRate = 60;
    data.keyframeInterval = 1;
    REQUIRE(ors::Config::gopFrameCount(data) == 60);
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
    REQUIRE(config.data().filenameTemplate == QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"));
    REQUIRE(config.data().filenamePrefix == QStringLiteral("錄製"));
    REQUIRE(config.data().filenameStartNumber == 1);
    REQUIRE(config.data().container == QStringLiteral("mp4"));
    REQUIRE(config.data().microphoneInputSource == QStringLiteral("stereo"));
    REQUIRE(config.data().captureIncludeCursor);
    REQUIRE(config.data().captureImageFormat == QStringLiteral("png"));
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
    REQUIRE(root.contains(QStringLiteral("recording")));
    REQUIRE(root.contains(QStringLiteral("capture")));
    const QJsonObject capture = root.value(QStringLiteral("capture")).toObject();
    REQUIRE(capture.value(QStringLiteral("includeCursor")).toBool());
    REQUIRE(capture.value(QStringLiteral("imageFormat")).toString() == QStringLiteral("png"));
}

TEST_CASE("capture.imageFormat jpeg aliases to jpg")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("alias.json"));

    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QByteArrayLiteral(
        "{ \"version\": 1, \"capture\": { \"includeCursor\": false, \"imageFormat\": \"jpeg\" } }"));
    file.close();

    ors::Config loaded;
    REQUIRE(loaded.load(path));
    REQUIRE_FALSE(loaded.data().captureIncludeCursor);
    REQUIRE(loaded.data().captureImageFormat == QStringLiteral("jpg"));
}

TEST_CASE("normalizedCaptureImageFormat maps aliases and fallbacks")
{
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("JPEG")) == QStringLiteral("jpg"));
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("bmp")) == QStringLiteral("bmp"));
    REQUIRE(ors::Config::normalizedCaptureImageFormat(QStringLiteral("webp")) == QStringLiteral("png"));
}

TEST_CASE("Empty output directory resolves to movies or home")
{
    ors::ConfigData data;
    const QString resolved = ors::Config::resolvedOutputDirectory(data);
    REQUIRE_FALSE(resolved.isEmpty());
}

TEST_CASE("alignCaptureSize snaps to 8x4")
{
    ors::ConfigData data;
    data.resolutionAlign = QStringLiteral("8x4");
    int width = 1920;
    int height = 1081;
    ors::Config::alignCaptureSize(data, width, height);
    REQUIRE(width == 1920);
    REQUIRE(height == 1080);
}

TEST_CASE("videoBitrateKbps scales with quality")
{
    ors::ConfigData data;
    data.frameRate = 30;
    data.videoQuality = QStringLiteral("low");
    const int low = ors::Config::videoBitrateKbps(data, 1280, 720);
    data.videoQuality = QStringLiteral("very-high");
    const int high = ors::Config::videoBitrateKbps(data, 1280, 720);
    REQUIRE(high > low);
}
