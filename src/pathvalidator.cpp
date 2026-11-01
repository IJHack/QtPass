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
  return storeRelativePath(storeRoot, candidate).has_value();
}

auto PathValidator::storeRelativePath(const QString &storeRoot,
                                      const QString &candidate)
    -> std::optional<QString> {
  if (storeRoot.isEmpty() || candidate.isEmpty()) {
    return std::nullopt;
  }
  const QString rootCanon = QDir(storeRoot).canonicalPath();
  if (rootCanon.isEmpty()) {
    return std::nullopt;
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
        return std::nullopt;
      }
    }
    const QString parentCanon = parent.canonicalPath();
    if (parentCanon.isEmpty()) {
      return std::nullopt;
    }
    resolved = QDir::cleanPath(parentCanon + QLatin1Char('/') +
                               tail.join(QLatin1Char('/')));
  }

  if (resolved.isEmpty()) {
    return std::nullopt;
  }
  if (resolved == rootCanon) {
    return QString();
  }
  if (!resolved.startsWith(rootCanon + QLatin1Char('/'))) {
    return std::nullopt;
  }
  return resolved.mid(rootCanon.size() + 1);
}

auto PathValidator::isTargetSpelled(const QString &storeRoot,
                                    const QString &path) -> bool {
  if (storeRoot.isEmpty() || !QDir::isAbsolutePath(path)) {
    return false;
  }
#ifdef Q_OS_WIN
  constexpr auto cs = Qt::CaseInsensitive;
#else
  constexpr auto cs = Qt::CaseSensitive;
#endif
  const QString root = QDir::cleanPath(storeRoot);
  const QString canonical = QDir(root).canonicalPath();
  if (canonical.isEmpty() || canonical.compare(root, cs) == 0) {
    return false;
  }
  const auto atOrUnder = [cs](const QString &p, const QString &dir) {
    const QString prefix =
        dir.endsWith(QLatin1Char('/')) ? dir : dir + QLatin1Char('/');
    return p.compare(dir, cs) == 0 || p.startsWith(prefix, cs);
  };
  const QString cleaned = QDir::cleanPath(path);
  return !atOrUnder(cleaned, root) && atOrUnder(cleaned, canonical);
}
