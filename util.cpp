#include "util.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/time.h>
#endif
QProcessEnvironment Util::_env;
bool Util::_envInitialised;

/**
 * @brief Util::initialiseEnvironment
 */
void Util::initialiseEnvironment() {
  if (!_envInitialised) {
    _env = QProcessEnvironment::systemEnvironment();
#ifdef __APPLE__
    // TODO(annejan) checks here
    QString path = _env.value("PATH");

    if (!path.contains("/usr/local/MacGPG2/bin") &&
        QFile("/usr/local/MacGPG2/bin").exists())
      path += ":/usr/local/MacGPG2/bin";
    if (!path.contains("/usr/local/bin"))
      path += ":/usr/local/bin";
    _env.insert("PATH", path);
#endif
    _envInitialised = true;
  }
}

/**
 * @brief Util::findPasswordStore
 * @return
 */
QString Util::findPasswordStore() {
  QString path;
  initialiseEnvironment();
  if (_env.contains("PASSWORD_STORE_DIR")) {
    path = _env.value("PASSWORD_STORE_DIR");
  } else {
#ifdef Q_OS_WIN
    path = QDir::homePath() + QDir::separator() + "password-store" +
           QDir::separator();
#else
    path = QDir::homePath() + QDir::separator() + ".password-store" +
           QDir::separator();
#endif
  }
  return Util::normalizeFolderPath(path);
}

/**
 * @brief Util::normalizeFolderPath
 * @param path
 * @return
 */
QString Util::normalizeFolderPath(QString path) {
  if (!path.endsWith("/") && !path.endsWith(QDir::separator()))
    path += QDir::separator();
  return path;
}

QString Util::findBinaryInPath(QString binary) {
  initialiseEnvironment();

  QString ret = "";

  binary.prepend(QDir::separator());

  if (_env.contains("PATH")) {
    QString path = _env.value("PATH");

    QStringList entries;
#ifndef Q_OS_WIN
    entries = path.split(':');
    if (entries.length() < 2) {
#endif
      entries = path.split(';');
#ifndef Q_OS_WIN
    }
#endif

    foreach (QString entry, entries) {
      QScopedPointer<QFileInfo> qfi(new QFileInfo(entry.append(binary)));
#ifdef Q_OS_WIN
      if (!qfi->exists())
        qfi.reset(new QFileInfo(entry.append(".exe")));

#endif
      qDebug() << entry;
      if (!qfi->isExecutable())
        continue;

      ret = qfi->absoluteFilePath();
      break;
    }
  }

  return ret;
}

/**
 * @brief Util::checkConfig
 * @param passStore
 * @param passExecutable
 * @param gpgExecutable
 * @return
 */
bool Util::checkConfig(QString passStore, QString passExecutable,
                       QString gpgExecutable) {
  return !QFile(passStore + ".gpg-id").exists() ||
         (!QFile(passExecutable).exists() && !QFile(gpgExecutable).exists());
}

/**
 * @brief Util::qSleep
 * @param ms
 */
void Util::qSleep(int ms) {
#ifdef Q_OS_WIN
  Sleep(uint(ms));
#else
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000 * 1000};
  nanosleep(&ts, NULL);
#endif
}
