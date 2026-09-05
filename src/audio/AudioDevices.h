#pragma once

#include <QString>
#include <QVector>

namespace ors {

struct AudioDeviceInfo {
    QString id;
    QString name;
};

#ifdef Q_OS_WIN
QVector<AudioDeviceInfo> listCaptureDevices();
#else
inline QVector<AudioDeviceInfo> listCaptureDevices()
{
    return {};
}
#endif

} // namespace ors
