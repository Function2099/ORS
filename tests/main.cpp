#include <QGuiApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("ORS"));
    QGuiApplication::setApplicationName(QStringLiteral("ORS-tests"));
    return Catch::Session().run(argc, argv);
}
