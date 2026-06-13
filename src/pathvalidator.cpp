// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class PathValidator
 * @brief Password-store path-boundary checks implementation.
 *
 * @see pathvalidator.h
 */

#include "pathvalidator.h"
#include <QDir>
#include <QFileInfo>
#include <QStringList>

auto PathValidator::isPathInStore(const QString &storeRoot,
                                  const QString &candidate) -> bool {
  if (storeRoot.isEmpty() || candidate.isEmpty()) {
    return false;
  }
  const QString rootCanon = QDir(storeRoot).canonicalPath();
  if (rootCanon.isEmpty()) {
    return false;
  }

  QString resolved;
  const QFileInfo fi(QDir::cleanPath(QFileInfo(candidate).absoluteFilePath()));
  if (fi.exists()) {
    resolved = fi.canonicalFilePath();
  } else {
    // Walk up to the nearest existing ancestor so we can canonicalise it
    // (resolves any symlinks on the way to the store root), then re-append
    // the leaf components below that ancestor.
    QDir parent = fi.dir();
    QStringList tail;
    tail.prepend(fi.fileName());
    while (!parent.exists()) {
      tail.prepend(parent.dirName());
      // The filesystem root always exists, so the loop terminates there
      // before cdUp() can fail; this guard is defensive against a
      // pathological QDir that can neither exist nor ascend.
      if (!parent.cdUp()) {
        return false;
      }
    }
    const QString parentCanon = parent.canonicalPath();
    if (parentCanon.isEmpty()) {
      return false;
    }
    resolved = QDir::cleanPath(parentCanon + QLatin1Char('/') +
                               tail.join(QLatin1Char('/')));
  }

  if (resolved.isEmpty()) {
    return false;
  }
  return resolved == rootCanon ||
         resolved.startsWith(rootCanon + QLatin1Char('/'));
}
