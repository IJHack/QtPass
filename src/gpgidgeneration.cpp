// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidgeneration.h"
#include "qtpasslogging.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QSettings>

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

/// One lock for every read-compare-write on the generation record within
/// this process (the re-encryption worker reads while the interface may
/// write); between processes the lock file next to the record does the same
/// (see Transaction).
QMutex &recordLock() {
  static QMutex mutex;
  return mutex;
}

/// How long a transaction waits for another process to let go of the record.
constexpr int kRecordLockTimeoutMs = 3000;

/// What the record keeps per list: the highest generation accepted or
/// written, and the SHA-256 of the exact bytes of that generation once they
/// are known (empty after a reservation, until the writer records them or a
/// reader accepts them).
struct Entry {
  qint64 generation = 0;
  QString digest;
};
using Entries = QMap<QString, Entry>;

const QString kRecordName = QStringLiteral("QtPass-gpgid-generations.json");
constexpr int kRecordFormat = 1;

/// Security state of its own, apart from the user's configuration, in a
/// file QtPass reads and writes itself: no settings cache between the read
/// and the write, no registry (on Windows a QSettings "file" is a registry
/// path). It lives where QSettings would put an ini file of this
/// organisation, so the platform's convention applies, and so does the
/// redirection the tests give that path.
auto recordPath() -> QString {
  const QSettings probe(QSettings::IniFormat, QSettings::UserScope,
                        QCoreApplication::organizationName().isEmpty()
                            ? QStringLiteral("IJHack")
                            : QCoreApplication::organizationName(),
                        QStringLiteral("QtPass-gpgid-generations"));
  return QFileInfo(probe.fileName()).absolutePath() + QLatin1Char('/') +
         kRecordName;
}

/// The record as on disk: nothing where there is no file yet, the entries
/// otherwise; nothing with @p error set when the file cannot be read or is
/// not a record ("unknown", never "never seen").
auto loadRecord(const QString &path, QString *error) -> std::optional<Entries> {
  Entries entries;
  if (!QFileInfo::exists(path)) {
    return entries;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = tr("The generation record of the recipient lists, %1, cannot "
                  "be accessed.")
                   .arg(path);
    return std::nullopt;
  }
  const auto unreadable = [&] {
    if (error)
      *error = tr("The generation record of the recipient lists, %1, is not "
                  "readable. Signed recipient lists are not accepted until "
                  "it is repaired or removed (which forgets what was accepted "
                  "before).")
                   .arg(path);
    return std::nullopt;
  };
  QJsonParseError parse{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
    return unreadable();
  }
  const QJsonObject root = doc.object();
  if (root.value(QLatin1String("format")).toInt() != kRecordFormat ||
      !root.value(QLatin1String("lists")).isObject()) {
    return unreadable();
  }
  const QJsonObject lists = root.value(QLatin1String("lists")).toObject();
  for (auto it = lists.constBegin(); it != lists.constEnd(); ++it) {
    if (!it.value().isObject()) {
      return unreadable();
    }
    const QJsonObject item = it.value().toObject();
    // The generation travels as a string: JSON numbers are doubles, and the
    // grammar admits 18 digits.
    const QString generation =
        item.value(QLatin1String("generation")).toString();
    bool ok = !generation.isEmpty() && generation.size() <= kMaxDigits;
    for (const QChar c : generation) {
      ok = ok && c >= QLatin1Char('0') && c <= QLatin1Char('9');
    }
    if (!ok) {
      return unreadable();
    }
    Entry entry;
    entry.generation = generation.toLongLong();
    entry.digest = item.value(QLatin1String("sha256")).toString();
    entries.insert(it.key(), entry);
  }
  return entries;
}

/// The record written through, whole, in place. Nothing is cached: a
/// failed write leaves the next transaction reading what is on disk.
auto saveRecord(const QString &path, const Entries &entries, QString *error)
    -> bool {
  QJsonObject lists;
  for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
    QJsonObject item;
    item.insert(QLatin1String("generation"),
                QString::number(it.value().generation));
    if (!it.value().digest.isEmpty()) {
      item.insert(QLatin1String("sha256"), it.value().digest);
    }
    lists.insert(it.key(), item);
  }
  QJsonObject root;
  root.insert(QLatin1String("format"), kRecordFormat);
  root.insert(QLatin1String("lists"), lists);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error)
      *error = tr("The generation record of the recipient lists, %1, cannot "
                  "be written.")
                   .arg(path);
    return false;
  }
  file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
  const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (file.write(bytes) != bytes.size() || !file.commit()) {
    qCWarning(lcQtPass) << "Could not write the .gpg-id generation record"
                        << path << file.errorString();
    if (error)
      *error = tr("The generation record of the recipient lists, %1, cannot "
                  "be written.")
                   .arg(path);
    return false;
  }
  return true;
}

/// A transaction on the record: the process mutex, then the lock file next
/// to the record against a second QtPass (a build without the
/// single-instance guard, a Flatpak next to a native one), then the record
/// read fresh from disk. `ok()` says whether all of that succeeded; when
/// not, nothing is read or written and the verdict is RecordUnavailable.
struct Transaction {
  QMutexLocker<QMutex> mutex{&recordLock()};
  QString path{recordPath()};
  // The record's folder may not exist before its first write; the lock file
  // needs it now.
  bool haveFolder{QDir().mkpath(QFileInfo(path).absolutePath())};
  QLockFile lock{path + QStringLiteral(".lock")};
  bool locked{haveFolder && lock.tryLock(kRecordLockTimeoutMs)};
  QString problem;
  std::optional<Entries> entries;

  Transaction() {
    if (!locked) {
      problem = tr("The generation record of the recipient lists, %1, could "
                   "not be locked: another QtPass may be using it, or its "
                   "folder cannot be written.")
                    .arg(path);
      return;
    }
    entries = loadRecord(path, &problem);
  }

  [[nodiscard]] auto ok(QString *error) const -> bool {
    if (entries) {
      return true;
    }
    if (error)
      *error = problem;
    return false;
  }

  /// The remembered generation for @p key, 0 when never seen.
  [[nodiscard]] auto generation(const QString &key) const -> qint64 {
    return entries->value(key).generation;
  }

  /// The remembered digest for @p key; nothing when none was recorded (a
  /// reservation not yet written, or a record from before digests).
  [[nodiscard]] auto digest(const QString &key) const
      -> std::optional<QString> {
    const QString value = entries->value(key).digest;
    if (value.isEmpty()) {
      return std::nullopt;
    }
    return value;
  }

  /// Write @p generation with @p digest (empty: none) for @p key through
  /// to disk.
  auto remember(const QString &key, qint64 generation, const QString &digest,
                QString *error) -> bool {
    Entries updated = *entries;
    updated[key] = Entry{generation, digest};
    if (!saveRecord(path, updated, error)) {
      return false;
    }
    entries = std::move(updated);
    return true;
  }
};

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

auto GpgIdGeneration::recordFile() -> QString { return recordPath(); }

auto GpgIdGeneration::digest(const QByteArray &contents) -> QString {
  return QString::fromLatin1(
      QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());
}

auto GpgIdGeneration::remembered(const QString &gpgIdFile, QString *error)
    -> std::optional<qint64> {
  Transaction t;
  if (!t.ok(error)) {
    return std::nullopt;
  }
  return t.generation(key(gpgIdFile));
}

auto GpgIdGeneration::accept(const QString &gpgIdFile,
                             const QByteArray &contents,
                             const QString &storeRoot, QString *error)
    -> Verdict {
  QString why;
  const std::optional<Header> header = parse(contents, &why);
  if (!header) {
    if (error)
      *error = tr("The signed recipient list %1 is not one to trust: %2")
                   .arg(gpgIdFile, why);
    return Verdict::Malformed;
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
      return Verdict::WrongFolder;
    }
  }
  Transaction t;
  if (!t.ok(error)) {
    return Verdict::RecordUnavailable;
  }
  const QString k = key(gpgIdFile);
  const qint64 last = t.generation(k);
  const qint64 generation = header->generation;
  // How a holder of the signing key gets through. At the ceiling of the
  // grammar no newer list can be written (reserveNext() refuses), so the
  // record has to go first.
  const auto wayOut = [&](const QString &saving) {
    if (last >= kMaxGeneration)
      return tr("Generation %1 is the highest there is, so no newer list "
                "can be written here: removing %2 forgets what this device "
                "accepted before, after which a holder of the signing key "
                "gets through by opening Users and %3.")
          .arg(last)
          .arg(t.path, saving);
    return tr("A holder of the signing key gets through by opening Users "
              "and %1, which writes generation %2. Removing %3 forgets what "
              "this device accepted before.")
        .arg(saving)
        .arg(last + 1)
        .arg(t.path);
  };
  if (generation < last) {
    if (!header->folder) {
      // Below what was accepted here and with no folder line to say it was
      // written here at all: an authentic headerless pair from any folder's
      // history would pass as this folder's rollback otherwise.
      if (error)
        *error =
            tr("The signed recipient list %1 carries no generation line, "
               "while generation %2 was accepted here before. pass writes no "
               "generation line (also through QtPass's pass backend), nor did "
               "QtPass before 2.0; without one the list may also have been "
               "written for another folder of the store and copied here. %3")
                .arg(gpgIdFile)
                .arg(last)
                .arg(wayOut(tr("selecting the recipients afresh and saving")));
      return Verdict::Unbound;
    }
    if (error)
      *error =
          tr("The signed recipient list %1 is generation %2, older than "
             "generation %3, the last one QtPass accepted here. It may have "
             "been put back by someone else. %4")
              .arg(gpgIdFile)
              .arg(generation)
              .arg(last)
              .arg(wayOut(tr("saving the recipients: the preselected "
                             "recipients there are this list's, so remove "
                             "anyone who should no longer have access first")));
    return Verdict::Rollback;
  }
  const QString bytes = digest(contents);
  if (generation == 0) {
    // Nothing accepted here yet and a list without a generation line: pass
    // writes those (also through QtPass's pass backend), for every change,
    // so the bytes differing from last time is the normal course of a
    // store kept with pass, not a conflict. There is no freshness to keep
    // at generation 0 (SECURITY.md); it begins with the first list QtPass
    // writes.
    return Verdict::Accepted;
  }
  if (generation == last) {
    // The same generation is the same list only if the bytes are: two
    // devices both making 19 from 18 is Git's conflict, and the device that
    // accepted one of them does not take the other for it; nor a swap. A
    // record without a digest (a reservation whose write was not recorded)
    // takes these bytes as the ones.
    const std::optional<QString> known = t.digest(k);
    if (known && *known != bytes) {
      if (error)
        *error = tr("The signed recipient list %1 is generation %2, the same "
                    "generation as a different list this device accepted "
                    "before. Either two devices saved recipients at the same "
                    "time (Git will have shown the conflict), or an authentic "
                    "list of that generation was swapped in. %3")
                     .arg(gpgIdFile)
                     .arg(generation)
                     .arg(wayOut(tr("checking the recipients and saving")));
      return Verdict::Conflict;
    }
    if (!known && !t.remember(k, generation, bytes, error)) {
      return Verdict::RecordUnavailable;
    }
    return Verdict::Accepted;
  }
  if (!t.remember(k, generation, bytes, error)) {
    return Verdict::RecordUnavailable;
  }
  return Verdict::Accepted;
}

auto GpgIdGeneration::reserveNext(const QString &gpgIdFile,
                                  std::optional<qint64> verifiedOnDisk,
                                  QString *error) -> std::optional<qint64> {
  Transaction t;
  if (!t.ok(error)) {
    return std::nullopt;
  }
  const QString k = key(gpgIdFile);
  const qint64 last = t.generation(k);
  const qint64 base = qMax(last, verifiedOnDisk.value_or(0));
  if (base >= kMaxGeneration) {
    if (error)
      *error = tr("The recipient list %1 has reached generation %2, the "
                  "highest there is; the list cannot be written.")
                   .arg(gpgIdFile)
                   .arg(base);
    return std::nullopt;
  }
  const qint64 generation = base + 1;
  // No digest until the bytes are written: the previous generation's must
  // not pass for this one's.
  if (!t.remember(k, generation, QString(), error)) {
    return std::nullopt;
  }
  return generation;
}

auto GpgIdGeneration::recordWritten(const QString &gpgIdFile,
                                    const QByteArray &contents, QString *error)
    -> bool {
  const std::optional<Header> header = parse(contents, error);
  if (!header) {
    return false;
  }
  Transaction t;
  if (!t.ok(error)) {
    return false;
  }
  const QString k = key(gpgIdFile);
  const qint64 last = t.generation(k);
  if (header->generation != last) {
    // Not the generation reserved last: something else moved the record in
    // between; whatever is there is not overwritten with these bytes.
    if (error)
      *error = tr("The generation record of the recipient lists, %1, has "
                  "moved on to generation %2 while generation %3 was being "
                  "written.")
                   .arg(t.path)
                   .arg(last)
                   .arg(header->generation);
    return false;
  }
  return t.remember(k, last, digest(contents), error);
}
