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

const QByteArray GpgIdGeneration::kGenerationPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Generation: ");
const QByteArray GpgIdGeneration::kFolderPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Folder: ");
const qint64 GpgIdGeneration::kMaxGeneration = 999999999999999999LL;

namespace {
constexpr int kMaxDigits = 18;
const QByteArray kOurComment = QByteArrayLiteral("# QtPass-GpgId-");

auto tr(const char *text) -> QString {
  return QCoreApplication::translate("GpgIdGeneration", text);
}

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

/// Once the record failed to parse, Qt's process-wide cache of the file goes
/// on as if it were empty, which would turn "unreadable" into "never seen";
/// so a format error is remembered for the rest of the process.
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
      *error = tr("The generation record of the recipient lists, %1, is not "
                  "readable. Signed recipient lists are not accepted until "
                  "it is repaired or removed (which forgets what was accepted "
                  "before) and QtPass is started again.")
                   .arg(record.fileName());
    return std::nullopt;
  }
  if (record.status() != QSettings::NoError) {
    if (error)
      *error = tr("The generation record of the recipient lists, %1, cannot "
                  "be accessed.")
                   .arg(record.fileName());
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
      *error = tr("The generation record of the recipient lists, %1, cannot "
                  "be written.")
                   .arg(record.fileName());
    return false;
  }
  return true;
}

/// Store-relative, `/`-separated, canonical, `.` for the root; nothing when
/// @p path is not under @p root.
auto relativeFolder(const QString &path, const QString &root)
    -> std::optional<QString> {
  QString folder = QFileInfo(path).absoluteDir().canonicalPath();
  if (folder.isEmpty()) {
    folder = QFileInfo(path).absolutePath();
  }
  QString store = QFileInfo(root).canonicalFilePath();
  if (store.isEmpty()) {
    store = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
  }
  folder = QDir::fromNativeSeparators(QDir::cleanPath(folder));
  store = QDir::fromNativeSeparators(QDir::cleanPath(store));
#ifdef Q_OS_WIN
  folder = folder.toLower();
  store = store.toLower();
#endif
  if (folder == store) {
    return QStringLiteral(".");
  }
  const QString prefix =
      store.endsWith(QLatin1Char('/')) ? store : store + QLatin1Char('/');
  if (!folder.startsWith(prefix)) {
    return std::nullopt;
  }
  return folder.mid(prefix.size());
}
} // namespace

auto GpgIdGeneration::parse(const QByteArray &contents, QString *error)
    -> std::optional<Header> {
  std::optional<qint64> generation;
  std::optional<QString> folder;
  for (const QByteArray &rawLine : contents.split('\n')) {
    QByteArray line = rawLine;
    if (line.endsWith('\r')) {
      line.chop(1);
    }
    // Only a line under our name is ours; any other comment is somebody
    // else's. But a near miss under the same name is suspicious, not
    // ignorable.
    if (!line.startsWith(kOurComment)) {
      continue;
    }
    if (line.startsWith(kGenerationPrefix)) {
      const QByteArray digits = line.mid(kGenerationPrefix.size());
      bool ok = digits.size() >= 1 && digits.size() <= kMaxDigits;
      for (const char c : digits) {
        ok = ok && c >= '0' && c <= '9';
      }
      if (!ok || generation) {
        if (error)
          *error = generation
                       ? tr("The list carries more than one generation line.")
                       : tr("The generation line is malformed: %1")
                             .arg(QString::fromUtf8(line));
        return std::nullopt;
      }
      generation = digits.toLongLong();
      continue;
    }
    if (line.startsWith(kFolderPrefix)) {
      const QByteArray value = line.mid(kFolderPrefix.size());
      if (value.isEmpty() || folder) {
        if (error)
          *error = folder ? tr("The list carries more than one folder line.")
                          : tr("The folder line is malformed: %1")
                                .arg(QString::fromUtf8(line));
        return std::nullopt;
      }
      folder = QString::fromUtf8(value);
      continue;
    }
    if (error)
      *error =
          tr("The header line is malformed: %1").arg(QString::fromUtf8(line));
    return std::nullopt;
  }
  if (generation && !folder) {
    if (error)
      *error = tr("The list carries a generation line but no folder line.");
    return std::nullopt;
  }
  Header header;
  header.generation = generation.value_or(0);
  header.folder = folder;
  return header;
}

auto GpgIdGeneration::header(qint64 generation, const QString &folder)
    -> QByteArray {
  return kGenerationPrefix + QByteArray::number(generation) + '\n' +
         kFolderPrefix + folder.toUtf8() + '\n';
}

auto GpgIdGeneration::withHeader(qint64 generation, const QString &folder,
                                 const QByteArray &recipients) -> QByteArray {
  return header(generation, folder) + recipients;
}

auto GpgIdGeneration::folderOf(const QString &gpgIdFile,
                               const QString &storeRoot)
    -> std::optional<QString> {
  return relativeFolder(gpgIdFile, storeRoot);
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

auto GpgIdGeneration::recordFile() -> QString {
  return openRecord()->fileName();
}

auto GpgIdGeneration::remembered(const QString &gpgIdFile, QString *error)
    -> std::optional<qint64> {
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  return readRemembered(*record, key(gpgIdFile), error);
}

auto GpgIdGeneration::accept(const QString &gpgIdFile,
                             const QByteArray &contents,
                             const QString &storeRoot, QString *error) -> bool {
  QString why;
  const std::optional<Header> header = parse(contents, &why);
  if (!header) {
    if (error)
      *error = why;
    return false;
  }
  // A pair is only valid where it was written for: copied into another
  // folder it is a rollback in disguise (that folder's first list).
  if (header->folder) {
    const std::optional<QString> here = folderOf(gpgIdFile, storeRoot);
    if (!here || *here != *header->folder) {
      if (error)
        *error = tr("The signed recipient list %1 was written for the folder "
                    "\"%2\" of the store, not for \"%3\", and is not used. It "
                    "may have been copied here by someone else; if the folder "
                    "was moved or renamed instead, a holder of the signing key "
                    "opens Users on it and saves the recipients, which binds "
                    "the list to where it is now.")
                     .arg(gpgIdFile, *header->folder,
                          here.value_or(QStringLiteral("?")));
      return false;
    }
  }
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  const QString k = key(gpgIdFile);
  const std::optional<qint64> last = readRemembered(*record, k, error);
  if (!last) {
    return false;
  }
  const qint64 generation = header->generation;
  if (generation < *last) {
    if (error) {
      const QString wayOut =
          tr("A holder of the signing key gets through by opening Users and "
             "saving the recipients, which writes generation %1: the "
             "preselected recipients there are this list's, so remove "
             "anyone who should no longer have access first. Removing %2 "
             "forgets what this device accepted before.")
              .arg(*last + 1)
              .arg(record->fileName());
      if (generation == 0) {
        *error = tr("The signed recipient list %1 carries no generation "
                    "line, while generation %2 was accepted here before. "
                    "pass writes no generation line (also through QtPass's "
                    "pass backend), nor did QtPass before 2.0. %3")
                     .arg(gpgIdFile)
                     .arg(*last)
                     .arg(wayOut);
      } else {
        *error = tr("The signed recipient list %1 is generation %2, older "
                    "than generation %3, the last one QtPass accepted here. "
                    "It may have been put back by someone else. %4")
                     .arg(gpgIdFile)
                     .arg(generation)
                     .arg(*last)
                     .arg(wayOut);
      }
    }
    return false;
  }
  if (generation > *last &&
      !writeRemembered(*record, k, generation, *last, error)) {
    return false;
  }
  return true;
}

auto GpgIdGeneration::reserveNext(const QString &gpgIdFile,
                                  std::optional<qint64> verifiedOnDisk,
                                  QString *error) -> std::optional<qint64> {
  QMutexLocker lock(&recordLock());
  auto record = openRecord();
  const QString k = key(gpgIdFile);
  const std::optional<qint64> last = readRemembered(*record, k, error);
  if (!last) {
    return std::nullopt;
  }
  const qint64 base = qMax(*last, verifiedOnDisk.value_or(0));
  if (base >= kMaxGeneration) {
    if (error)
      *error = tr("The recipient list %1 has reached generation %2, the "
                  "highest there is; the list cannot be written.")
                   .arg(gpgIdFile)
                   .arg(base);
    return std::nullopt;
  }
  const qint64 generation = base + 1;
  if (!writeRemembered(*record, k, generation, *last, error)) {
    return std::nullopt;
  }
  return generation;
}
