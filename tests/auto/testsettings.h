// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TESTS_AUTO_TESTSETTINGS_H_
#define TESTS_AUTO_TESTSETTINGS_H_

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtGlobal>

/**
 * @brief Redirect QSettings (and thus QtPassSettings) to a throw-away
 * directory for the lifetime of the test process.
 *
 * Call this as the first statement of initTestCase(), before anything
 * touches QtPassSettings::getInstance(): the singleton fixes its file path
 * on first use. Without this, suites write to the user's live
 * ~/.config/IJHack/QtPass.conf (or the registry on Windows) and a suite that
 * is killed half-way leaves a temp store path behind in the real config.
 *
 * Both NativeFormat and IniFormat are redirected: on Linux/macOS the
 * QSettings(org, app) constructor uses NativeFormat regardless of
 * setDefaultFormat(); on Windows NativeFormat is the registry (setPath is a
 * no-op there), so the default format is switched to IniFormat as well.
 * QStandardPaths test mode keeps any other per-user location out of $HOME.
 */
inline void isolateTestSettings() {
  static QTemporaryDir dir; // lives until process exit
  if (!dir.isValid()) {
    // An empty path would make QSettings::setPath() point at "/", i.e. a
    // real, shared location. Abort rather than run against the wrong config.
    qFatal("isolateTestSettings: cannot create temporary settings dir: %s",
           qPrintable(dir.errorString()));
  }
  const QString userPath = QDir::cleanPath(dir.path());
  const QString systemPath = QDir::cleanPath(dir.filePath("system"));
  QStandardPaths::setTestModeEnabled(true);
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, userPath);
  QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, userPath);
  QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, systemPath);
  QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope,
                     systemPath);
}

#endif // TESTS_AUTO_TESTSETTINGS_H_
