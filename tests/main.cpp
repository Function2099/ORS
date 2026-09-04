#include <QCoreApplication>

#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ORS"));
    QCoreApplication::setApplicationName(QStringLiteral("ORS-tests"));
    return Catch::Session().run(argc, argv);
}
