#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace encoder {

// A signed macOS bundle is read-only; keep editable configuration outside it.
inline QDir guiConfigDirectory(const QString& component) {
#if defined(Q_OS_MACOS)
  QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/encoder/" + component + "/config");
  if (QDir().mkpath(dir.absolutePath())) {
    QFile::setPermissions(dir.absolutePath(), QFileDevice::ReadOwner |
                          QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString name = component + ".conf";
    const QString destination = dir.filePath(name);
    if (!QFile::exists(destination)) {
      QFile::copy(QCoreApplication::applicationDirPath() +
                  "/../Resources/config/" + name, destination);
      QFile::setPermissions(destination, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
  }
  return dir;
#else
  Q_UNUSED(component);
  return QDir(QCoreApplication::applicationDirPath() + "/config");
#endif
}

} // namespace encoder
