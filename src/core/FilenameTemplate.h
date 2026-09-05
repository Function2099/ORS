#pragma once

#include <QDateTime>
#include <QString>

namespace ors {

struct FilenameContext {
    QString prefix;
    QString appName;
    QString userId;
    QString displayName;
};

FilenameContext defaultFilenameContext(const QString& prefix);

QString expandFilenameTemplate(
    const QString& filenameTemplate,
    const FilenameContext& context,
    const QDateTime& when,
    int number = 1);

QString expandFilenameTemplate(
    const QString& filenameTemplate,
    const QString& prefix,
    const QDateTime& when,
    int number = 1);

QString makeUniqueOutputPath(
    const QString& directory,
    const QString& filenameTemplate,
    const QString& prefix,
    int startNumber,
    const QString& extension,
    const QDateTime& when);

} // namespace ors
