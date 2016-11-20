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
#include "qtpasssettings.h"
QProcessEnvironment Util::_env;
bool Util::_envInitialised;

/**
 * @brief Util::initialiseEnvironment set the correct PATH for use with gpg, git
 * etc.
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
 * @brief Util::findPasswordStore look for common .password-store folder
 * location.
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
 * @brief Util::normalizeFolderPath let's always end folders with a
 * QDir::separator()
 * @param path
 * @return
 */
QString Util::normalizeFolderPath(QString path) {
  if (!path.endsWith("/") && !path.endsWith(QDir::separator()))
    path += QDir::separator();
  return path;
}

/**
 * @brief Util::findBinaryInPath search for executables.
 * @param binary
 * @return
 */
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
 * @brief Util::checkConfig do we have prequisite settings?
 * @return
 */
bool Util::checkConfig() {
  return !QFile(QDir(QtPassSettings::getPassStore()).filePath(".gpg-id")).exists() ||
         (!QFile(QtPassSettings::getPassExecutable()).exists() && !QFile(QtPassSettings::getGpgExecutable()).exists());
}

/**
 * @brief Util::qSleep because . . windows sleep.
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
