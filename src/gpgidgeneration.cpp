// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidgeneration.h"
#include "qtpasslogging.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <memory>

const QByteArray GpgIdGeneration::kPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Generation: ");

namespace {
constexpr int kMaxDigits = 18;

/// One lock for every read-compare-write on the generation record: a fresh
/// QSettings per transaction (the same QSettings object is not thread-safe,
/// and the re-encryption worker reads while the interface may write), and
/// no interleaving of two transactions.
QMutex &recordLock() {
  static QMutex mutex;
  return mutex;
}

/// Security state of its own, apart from the user's configuration: the
/// application's settings scope, a separate file, no fallback locations.
auto openRecord() -> std::unique_ptr<QSettings> {
  auto settings = std::make_unique<QSettings>(
      QSettings::defaultFormat(), QSettings::UserScope,
      QCoreApplication::organizationName().isEmpty()
          ? QStringLiteral("IJHack")
          : QCoreApplication::organizationName(),
      QStringLiteral("QtPass-gpgid-generations"));
  settings->setFallbacksEnabled(false);
  return settings;
}

auto statusText(QSettings::Status status) -> QString {
  return status == QSettings::AccessError
             ? QCoreApplication::translate("GpgIdGeneration",
                                           "the record cannot be accessed")
             : QCoreApplication::translate("GpgIdGeneration",
                                           "the record is not readable");
}

/// Once the record failed to parse, Qt's process-wide cache of the file
/// goes on as if it were empty, which would turn "unreadable" into "never
/// seen"; so a format error is remembered for the rest of the process.
bool recordCorrupt = false;

/// The remembered generation, read fresh; nothing when the record cannot be
/// read. Caller holds recordLock().
auto readRemembered(QSettings &record, const QString &key, QString *error)
    -> std::optional<qint64> {
  record.sync();
  if (record.status() == QSettings::FormatError) {
    recordCorrupt = true;
  }
  if (recordCorrupt) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists, %1, is not "
                   "readable. Signed recipient lists are not accepted until it "
                   "is repaired or removed (which forgets what was accepted "
                   "before) and QtPass is started again.")
                   .arg(record.fileName());
    return std::nullopt;
  }
  if (record.status() != QSettings::NoError) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists could not "
                   "be read (%1).")
                   .arg(statusText(record.status()));
    return std::nullopt;
  }
  return record.value(key, 0).toLongLong();
}

/// Write @p generation through to disk. Caller holds recordLock().
auto writeRemembered(QSettings &record, const QString &key, qint64 generation,
                     qint64 previous, QString *error) -> bool {
  record.setValue(key, generation);
  record.sync();
  if (record.status() != QSettings::NoError) {
    // Qt keeps the file's contents in a process-wide cache with the failed
    // write pending; put the previous value back so a later, successful
    // sync does not record a generation nobody accepted.
    if (previous == 0) {
      record.remove(key);
    } else {
      record.setValue(key, previous);
    }
    qCWarning(lcQtPass) << "Could not record the .gpg-id generation" << key
                        << "status" << record.status();
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists could not "
                   "be written (%1).")
                   .arg(statusText(record.status()));
    return false;
  }
  return true;
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

auto GpgIdGeneration::remembered(const QString &gpgIdFile, QString *error)
    -> std::optional<qint64> {
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  return readRemembered(*record, key(gpgIdFile), error);
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
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  const QString k = key(gpgIdFile);
  const std::optional<qint64> last = readRemembered(*record, k, error);
  if (!last) {
    return false;
  }
  if (*generation < *last) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The signed recipient list %1 is generation %2, older than "
                   "generation %3, the last one QtPass accepted here. If going "
                   "back to it is intended, open Users and save the list "
                   "again.")
                   .arg(gpgIdFile)
                   .arg(*generation)
                   .arg(*last);
    return false;
  }
  if (*generation > *last &&
      !writeRemembered(*record, k, *generation, *last, error)) {
    return false;
  }
  return true;
}

auto GpgIdGeneration::reserveNext(const QString &gpgIdFile, QString *error)
    -> std::optional<qint64> {
  qint64 onDisk = 0;
  QFile file(gpgIdFile);
  if (file.open(QIODevice::ReadOnly)) {
    onDisk = parse(file.readAll()).value_or(0);
  }
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  const QString k = key(gpgIdFile);
  const std::optional<qint64> last = readRemembered(*record, k, error);
  if (!last) {
    return std::nullopt;
  }
  const qint64 generation = qMax(onDisk, *last) + 1;
  if (!writeRemembered(*record, k, generation, *last, error)) {
    return std::nullopt;
  }
  return generation;
}
