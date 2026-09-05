#include "core/Config.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ORS"));
    QApplication::setApplicationName(QStringLiteral("ORS"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    ors::Config config;
    config.load();

    QTranslator translator;
    const QString language = config.data().language.isEmpty()
        ? QStringLiteral("zh_TW")
        : config.data().language;
    if (translator.load(QStringLiteral(":/i18n/ors_%1.qm").arg(language))) {
        app.installTranslator(&translator);
    }

    ors::MainWindow window;
    window.present();
    return app.exec();
}
