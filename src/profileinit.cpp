// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file profileinit.cpp
 * @brief Profile initialization utilities implementation.
 */

#include "profileinit.h"
#include "qtpasssettings.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

/**
 * @brief Check if a profile path is new (doesn't have .gpg-id yet).
 * @param path The profile path to check.
 * @return true if the path needs initialization.
 */
auto ProfileInit::needsInit(const QString &path) -> bool {
  QDir dir(path);
  if (!dir.exists()) {
    return false;
  }
  return !dir.exists(".gpg-id");
}

/**
 * @brief Initialize a new profile's password store.
 *
 * Creates .gpg-id file with the given recipients, and optionally
 * initializes git repository.
 *
 * Note: This is a synchronous implementation. For full pass initialization
 * with GPG encryption, use QtPassSettings::getPass()->Init() which handles
 * async GPG operations.
 *
 * @param path The profile path to initialize.
 * @param recipients List of GPG key IDs to encrypt for.
 * @param useGit Whether to also initialize git.
 * @return true if initialization was successful.
 */
auto ProfileInit::init(const QString &path, const QStringList &recipients,
                       bool useGit) -> bool {
  QDir dir(path);
  if (!dir.exists()) {
    return false;
  }

  if (recipients.isEmpty()) {
    return false;
  }

  QString gpgIdPath = dir.filePath(".gpg-id");
  QFile file(gpgIdPath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QTextStream stream(&file);
  for (const QString &recipient : recipients) {
    stream << recipient << "\n";
  }
  file.close();

  if (useGit) {
    QString prevStore = QtPassSettings::getPassStore();
    QtPassSettings::setPassStore(path);
    QtPassSettings::getPass()->GitInit();
    QtPassSettings::setPassStore(prevStore);
  }

  return true;
}