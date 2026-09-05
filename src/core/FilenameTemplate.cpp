#include "core/FilenameTemplate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>

#include <algorithm>

namespace ors {
namespace {

bool isHashToken(const QString& inner)
{
    return !inner.isEmpty() && inner == QString(inner.size(), QLatin1Char('#'));
}

QString paddedNumber(int number, int width)
{
    return QString::number(std::max(1, number)).rightJustified(std::max(1, width), QLatin1Char('0'));
}

QString currentUserId()
{
    QString user = qEnvironmentVariable("USERNAME");
    if (user.isEmpty()) {
        user = qEnvironmentVariable("USER");
    }
    return user.isEmpty() ? QStringLiteral("user") : user;
}

QString currentDisplayName()
{
    return currentUserId();
}

QString localizedDate(const QDateTime& when)
{
    QString text = QLocale().toString(when.date(), QLocale::LongFormat);
    if (text.isEmpty()) {
        text = QLocale().toString(when.date(), QLocale::ShortFormat);
    }
    const QString illegal = QStringLiteral("<>:\"/\\|?*");
    for (QChar ch : illegal) {
        text.replace(ch, QLatin1Char('-'));
    }
    return text.trimmed();
}

QString expandDateTokens(QString inner, const QDateTime& when)
{
    const QString year4 = when.toString(QStringLiteral("yyyy"));
    const QString year2 = when.toString(QStringLiteral("yy"));
    const QString month = when.toString(QStringLiteral("MM"));
    const QString day = when.toString(QStringLiteral("dd"));
    const QString hour = when.toString(QStringLiteral("HH"));
    const QString minute = when.toString(QStringLiteral("mm"));
    const QString second = when.toString(QStringLiteral("ss"));
    const QString millis = when.toString(QStringLiteral("zzz"));

    inner.replace(QLatin1String("YYYY"), year4);
    inner.replace(QLatin1String("yyyy"), year4);
    inner.replace(QLatin1String("YY"), year2);
    inner.replace(QLatin1String("yy"), year2);
    inner.replace(QLatin1String("MM"), month);
    inner.replace(QLatin1String("DD"), day);
    inner.replace(QLatin1String("dd"), day);
    inner.replace(QLatin1String("HH"), hour);
    inner.replace(QLatin1String("hh"), hour);
    inner.replace(QLatin1String("NN"), minute);
    inner.replace(QLatin1String("nn"), minute);
    inner.replace(QLatin1String("SS"), second);
    inner.replace(QLatin1String("ss"), second);
    inner.replace(QLatin1String("ZZZ"), millis);
    inner.replace(QLatin1String("zzz"), millis);
    inner.replace(QLatin1Char('Z'), millis);
    inner.replace(QLatin1Char('z'), millis);
    return inner;
}

QString expandBracketToken(const QString& inner, const FilenameContext& context, const QDateTime& when, int number)
{
    if (inner.compare(QLatin1String("Prefix"), Qt::CaseInsensitive) == 0) {
        return context.prefix;
    }
    if (inner.compare(QLatin1String("Name"), Qt::CaseInsensitive) == 0
        || inner.compare(QLatin1String("App"), Qt::CaseInsensitive) == 0) {
        return context.appName;
    }
    if (inner.compare(QLatin1String("User"), Qt::CaseInsensitive) == 0
        || inner.compare(QLatin1String("UserId"), Qt::CaseInsensitive) == 0) {
        return context.userId;
    }
    if (inner.compare(QLatin1String("DisplayName"), Qt::CaseInsensitive) == 0
        || inner.compare(QLatin1String("FullName"), Qt::CaseInsensitive) == 0) {
        return context.displayName;
    }
    if (inner.compare(QLatin1String("Date"), Qt::CaseInsensitive) == 0
        || inner.compare(QLatin1String("LocalizedDate"), Qt::CaseInsensitive) == 0) {
        return localizedDate(when);
    }
    if (isHashToken(inner)) {
        return paddedNumber(number, inner.size());
    }
    return expandDateTokens(inner, when);
}

QString replaceHashNumbers(const QString& text, int number)
{
    QString out;
    int i = 0;
    while (i < text.size()) {
        if (text.at(i) != QLatin1Char('#')) {
            out += text.at(i);
            ++i;
            continue;
        }
        int end = i;
        while (end < text.size() && text.at(end) == QLatin1Char('#')) {
            ++end;
        }
        out += paddedNumber(number, end - i);
        i = end;
    }
    return out;
}

bool templateHasNumberToken(const QString& filenameTemplate)
{
    int i = 0;
    while (i < filenameTemplate.size()) {
        if (filenameTemplate.at(i) == QLatin1Char('<')) {
            const int close = filenameTemplate.indexOf(QLatin1Char('>'), i + 1);
            if (close < 0) {
                return filenameTemplate.contains(QLatin1Char('#'));
            }
            if (isHashToken(filenameTemplate.mid(i + 1, close - i - 1))) {
                return true;
            }
            i = close + 1;
            continue;
        }
        if (filenameTemplate.at(i) == QLatin1Char('#')) {
            return true;
        }
        ++i;
    }
    return false;
}

QString sanitizeFilename(QString name)
{
    const QString illegal = QStringLiteral("<>:\"/\\|?*");
    for (QChar ch : illegal) {
        name.replace(ch, QLatin1Char('_'));
    }
    name.replace(QLatin1Char('\n'), QLatin1Char('_'));
    name.replace(QLatin1Char('\r'), QLatin1Char('_'));
    while (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.'))) {
        name.chop(1);
    }
    return name;
}

QString fallbackName(const FilenameContext& context, const QDateTime& when, int number)
{
    QString name = expandFilenameTemplate(
        QStringLiteral("<Prefix>_<YYYY_MM_DD_HH_NN_SS_Z>"),
        context,
        when,
        number);
    if (name.isEmpty()) {
        name = when.toString(QStringLiteral("ORS_yyyyMMdd_HHmmss"));
    }
    return name;
}

} // namespace

FilenameContext defaultFilenameContext(const QString& prefix)
{
    FilenameContext context;
    context.prefix = prefix;
    context.appName = QCoreApplication::applicationName();
    if (context.appName.isEmpty()) {
        context.appName = QStringLiteral("ORS");
    }
    context.userId = currentUserId();
    context.displayName = currentDisplayName();
    if (context.displayName.isEmpty()) {
        context.displayName = context.userId;
    }
    return context;
}

QString expandFilenameTemplate(
    const QString& filenameTemplate,
    const FilenameContext& context,
    const QDateTime& when,
    int number)
{
    const QString tmpl = filenameTemplate.trimmed();
    if (tmpl.isEmpty()) {
        return fallbackName(context, when, number);
    }
    if (!tmpl.contains(QLatin1Char('<')) && !tmpl.contains(QLatin1Char('#'))) {
        const QString stamped = when.toString(tmpl);
        return stamped.isEmpty() ? fallbackName(context, when, number) : stamped;
    }

    QString out;
    int i = 0;
    while (i < tmpl.size()) {
        if (tmpl.at(i) != QLatin1Char('<')) {
            out += tmpl.at(i);
            ++i;
            continue;
        }
        const int close = tmpl.indexOf(QLatin1Char('>'), i + 1);
        if (close < 0) {
            out += tmpl.mid(i);
            break;
        }
        out += expandBracketToken(tmpl.mid(i + 1, close - i - 1), context, when, number);
        i = close + 1;
    }
    out = replaceHashNumbers(out, number);
    return out.isEmpty() ? fallbackName(context, when, number) : out;
}

QString expandFilenameTemplate(
    const QString& filenameTemplate,
    const QString& prefix,
    const QDateTime& when,
    int number)
{
    return expandFilenameTemplate(filenameTemplate, defaultFilenameContext(prefix), when, number);
}

QString makeUniqueOutputPath(
    const QString& directory,
    const QString& filenameTemplate,
    const QString& prefix,
    int startNumber,
    const QString& extension,
    const QDateTime& when)
{
    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return {};
    }

    QString ext = extension.trimmed();
    if (!ext.isEmpty() && !ext.startsWith(QLatin1Char('.'))) {
        ext.prepend(QLatin1Char('.'));
    }
    if (ext.isEmpty()) {
        ext = QStringLiteral(".mp4");
    }

    const FilenameContext context = defaultFilenameContext(prefix);
    const bool numbered = templateHasNumberToken(filenameTemplate);
    const int start = std::max(1, startNumber);
    int sequence = start;
    for (int attempt = 0; attempt < 10000; ++attempt) {
        QString base = sanitizeFilename(
            expandFilenameTemplate(filenameTemplate, context, when, sequence));
        if (base.isEmpty()) {
            base = sanitizeFilename(fallbackName(context, when, sequence));
        }
        QString fileName = base + ext;
        if (!numbered && attempt > 0) {
            fileName = base + QLatin1Char('_') + QString::number(start + attempt - 1) + ext;
        }
        const QString path = dir.filePath(fileName);
        if (!QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
        ++sequence;
    }
    return {};
}

} // namespace ors
