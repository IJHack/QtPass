// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidgeneration.h"
#include "qtpasslogging.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

const QByteArray GpgIdGeneration::kPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Generation: ");

namespace {
constexpr int kMaxDigits = 18;

/// Security state of its own, apart from the user's configuration: the
/// application's settings scope, a separate file, no fallback locations.
auto store() -> QSettings * {
  static QSettings settings(QSettings::defaultFormat(), QSettings::UserScope,
                            QCoreApplication::organizationName().isEmpty()
                                ? QStringLiteral("IJHack")
                                : QCoreApplication::organizationName(),
                            QStringLiteral("QtPass-gpgid-generations"));
  settings.setFallbacksEnabled(false);
  return &settings;
}
} // namespace

auto GpgIdGeneration::parse(const QByteArray &contents, QString *error)
    -> std::optional<qint64> {
  std::optional<qint64> found;
  for (const QByteArray &rawLine : contents.split('\n')) {
    QByteArray line = rawLine;
    if (line.endsWith('\r')) {
      line.chop(1);
    }
    // Only a line that starts with the prefix is a generation line; any other
    // comment is somebody else's. But a near miss under the same name is
    // suspicious, not ignorable.
    if (!line.startsWith(QByteArrayLiteral("# QtPass-GpgId-Generation"))) {
      continue;
    }
    if (!line.startsWith(kPrefix)) {
      if (error)
        *error = QCoreApplication::translate(
                     "GpgIdGeneration", "The generation line is malformed: %1")
                     .arg(QString::fromUtf8(line));
      return std::nullopt;
    }
    const QByteArray digits = line.mid(kPrefix.size());
    bool ok = digits.size() >= 1 && digits.size() <= kMaxDigits;
    for (const char c : digits) {
      ok = ok && c >= '0' && c <= '9';
    }
    if (!ok) {
      if (error)
        *error = QCoreApplication::translate(
                     "GpgIdGeneration", "The generation line is malformed: %1")
                     .arg(QString::fromUtf8(line));
      return std::nullopt;
    }
    if (found) {
      if (error)
        *error = QCoreApplication::translate(
            "GpgIdGeneration", "The list carries more than one generation.");
      return std::nullopt;
    }
    found = digits.toLongLong();
  }
  return found.value_or(0);
}

auto GpgIdGeneration::header(qint64 generation) -> QByteArray {
  return kPrefix + QByteArray::number(generation) + '\n';
}

auto GpgIdGeneration::withHeader(qint64 generation,
                                 const QByteArray &recipients) -> QByteArray {
  return header(generation) + recipients;
}

auto GpgIdGeneration::key(const QString &gpgIdFile) -> QString {
  const QFileInfo info(gpgIdFile);
  // canonicalFilePath() is empty for a file that does not exist yet (a new
  // profile's list): its folder does, so canonicalise that and append.
  QString canonical = info.canonicalFilePath();
  if (canonical.isEmpty()) {
    const QString dir = info.absoluteDir().canonicalPath();
    canonical = (dir.isEmpty() ? info.absolutePath() : dir) + QLatin1Char('/') +
                info.fileName();
  }
  canonical = QDir::fromNativeSeparators(canonical);
#ifdef Q_OS_WIN
  canonical = canonical.toLower();
#endif
  return QString::fromLatin1(
      QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256)
          .toHex());
}

auto GpgIdGeneration::remembered(const QString &gpgIdFile) -> qint64 {
  QSettings *s = store();
  s->sync();
  return s->value(key(gpgIdFile), 0).toLongLong();
}

auto GpgIdGeneration::remember(const QString &gpgIdFile, qint64 generation)
    -> bool {
  QSettings *s = store();
  s->setValue(key(gpgIdFile), generation);
  s->sync();
  if (s->status() != QSettings::NoError) {
    qCWarning(lcQtPass) << "Could not record the .gpg-id generation for"
                        << gpgIdFile << "status" << s->status();
    return false;
  }
  return true;
}

auto GpgIdGeneration::accept(const QString &gpgIdFile,
                             const QByteArray &contents, QString *error)
    -> bool {
  QString why;
  const std::optional<qint64> generation = parse(contents, &why);
  if (!generation) {
    if (error)
      *error = why;
    return false;
  }
  const qint64 last = remembered(gpgIdFile);
  if (*generation < last) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The signed recipient list %1 is generation %2, older than "
                   "generation %3, the last one QtPass accepted here. If going "
                   "back to it is intended, open Users and save the list "
                   "again.")
                   .arg(gpgIdFile)
                   .arg(*generation)
                   .arg(last);
    return false;
  }
  if (*generation > last && !remember(gpgIdFile, *generation)) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "Could not record that generation %1 of %2 was accepted; "
                   "the list was not used.")
                   .arg(*generation)
                   .arg(gpgIdFile);
    return false;
  }
  return true;
}

auto GpgIdGeneration::next(const QString &gpgIdFile) -> qint64 {
  qint64 onDisk = 0;
  QFile file(gpgIdFile);
  if (file.open(QIODevice::ReadOnly)) {
    onDisk = parse(file.readAll()).value_or(0);
  }
  return qMax(onDisk, remembered(gpgIdFile)) + 1;
}
