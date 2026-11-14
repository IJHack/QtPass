// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements the `.gpg-id` generation header and the per-store record
 * of accepted generations.
 */
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
#include <QUuid>
#include <algorithm>

const QByteArray GpgIdGeneration::kGenerationPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Generation: ");
const QByteArray GpgIdGeneration::kFolderPrefix =
    QByteArrayLiteral("# QtPass-GpgId-Folder: ");
const QByteArray GpgIdGeneration::kStorePrefix =
    QByteArrayLiteral("# QtPass-GpgId-Store: ");
const qint64 GpgIdGeneration::kMaxGeneration = 999999999999999999LL;

namespace {
constexpr int kMaxDigits = 18;
const QByteArray kOurComment = QByteArrayLiteral("# QtPass-GpgId-");

// Not a QObject: messages live under the class name. lupdate gets that context
// from the class's own methods, but drops strings in the free functions and
// structs here, so those spell it out with QCoreApplication::translate().
auto tr(const char *text) -> QString {
  return QCoreApplication::translate("GpgIdGeneration", text);
}

/// Serialises read-compare-write on the record in this process (the
/// re-encryption worker reads while the UI may write); see Transaction for
/// the cross-process lock file.
QMutex &recordLock() {
  static QMutex mutex;
  return mutex;
}

/// How long a transaction waits for another process to let go of the record.
constexpr int kRecordLockTimeoutMs = 3000;

/// Per list: the highest generation accepted or written, and the SHA-256 of
/// its exact bytes once known (empty after a reservation).
struct Entry {
  qint64 generation = 0;
  QString digest;
};
using Entries = QMap<QString, Entry>;
/// Per store, by GpgIdGeneration::key() of its root: the store id this
/// device knows it by.
using Stores = QMap<QString, QString>;

/// Everything the record holds.
struct Record {
  Entries lists;
  Stores stores;
};

/// Whether @p id is a store id as QtPass writes it: 32 lowercase hex digits.
auto isStoreId(const QByteArray &id) -> bool {
  return id.size() == 32 && std::all_of(id.cbegin(), id.cend(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

const QString kRecordName = QStringLiteral("QtPass-gpgid-generations.json");
/// The store ids live in a file of their own, under the record's lock: a
/// QtPass from before store ids rewrites the record whole, and would drop
/// anything it does not know from it.
const QString kStoresName = QStringLiteral("QtPass-gpgid-stores.json");
constexpr int kRecordFormat = 1;

/// A file QtPass reads and writes itself, not QSettings: no cache between
/// read and write, no registry (on Windows a QSettings "file" is a registry
/// path). Placed where QSettings would put an ini file, so the platform
/// convention and the tests' redirection of that path both apply.
auto recordPath() -> QString {
  const QSettings probe(QSettings::IniFormat, QSettings::UserScope,
                        QCoreApplication::organizationName().isEmpty()
                            ? QStringLiteral("IJHack")
                            : QCoreApplication::organizationName(),
                        QStringLiteral("QtPass-gpgid-generations"));
  return QFileInfo(probe.fileName()).absolutePath() + QLatin1Char('/') +
         kRecordName;
}

/// The store ids' file, next to the record.
auto storesPath() -> QString {
  return QFileInfo(recordPath()).absolutePath() + QLatin1Char('/') +
         kStoresName;
}

/// One list's entry in the record, or nothing when it is malformed. The
/// generation travels as a string: JSON numbers are doubles, and the grammar
/// admits 18 digits.
auto recordEntry(const QJsonValue &value) -> std::optional<Entry> {
  if (!value.isObject()) {
    return std::nullopt;
  }
  const QJsonObject item = value.toObject();
  const QString generation = item.value(QLatin1String("generation")).toString();
  if (generation.isEmpty() || generation.size() > kMaxDigits) {
    return std::nullopt;
  }
  for (const QChar c : generation) {
    if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
      return std::nullopt;
    }
  }
  Entry entry;
  entry.generation = generation.toLongLong();
  entry.digest = item.value(QLatin1String("sha256")).toString();
  return entry;
}

/// The JSON object a record file holds, when it is one of @p format and
/// carries an object under @p name.
auto recordObject(const QByteArray &bytes, const QString &name)
    -> std::optional<QJsonObject> {
  QJsonParseError parse{};
  const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
  if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
    return std::nullopt;
  }
  const QJsonObject root = doc.object();
  const QJsonValue inner = root.value(name);
  if (root.value(QLatin1String("format")).toInt() != kRecordFormat ||
      !inner.isObject()) {
    return std::nullopt;
  }
  return inner.toObject();
}

/// The record's entries from its bytes, or nothing when any part of it is
/// not what QtPass writes.
auto parseRecord(const QByteArray &bytes) -> std::optional<Entries> {
  const std::optional<QJsonObject> byKey =
      recordObject(bytes, QStringLiteral("lists"));
  if (!byKey) {
    return std::nullopt;
  }
  Entries entries;
  for (auto it = byKey->constBegin(); it != byKey->constEnd(); ++it) {
    const std::optional<Entry> entry = recordEntry(it.value());
    if (!entry) {
      return std::nullopt;
    }
    entries.insert(it.key(), *entry);
  }
  return entries;
}

/// The store ids from their file's bytes, or nothing when any part of it is
/// not what QtPass writes.
auto parseStores(const QByteArray &bytes) -> std::optional<Stores> {
  const std::optional<QJsonObject> byKey =
      recordObject(bytes, QStringLiteral("stores"));
  if (!byKey) {
    return std::nullopt;
  }
  Stores stores;
  for (auto it = byKey->constBegin(); it != byKey->constEnd(); ++it) {
    const QString id =
        it.value().toObject().value(QLatin1String("id")).toString();
    if (!it.value().isObject() || !isStoreId(id.toLatin1())) {
      return std::nullopt;
    }
    stores.insert(it.key(), id);
  }
  return stores;
}

/// A record file as on disk, parsed by @p parse: empty where there is no
/// file yet; nothing with @p error set when the file cannot be read or does
/// not parse ("unknown", never "never seen").
template <typename T, typename Parse>
auto loadFile(const QString &path, Parse parse, QString *error)
    -> std::optional<T> {
  if (!QFileInfo::exists(path)) {
    return T{};
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists, %1, cannot "
                   "be accessed.")
                   .arg(path);
    return std::nullopt;
  }
  std::optional<T> value = parse(file.readAll());
  if (!value && error) {
    *error = QCoreApplication::translate(
                 "GpgIdGeneration",
                 "The generation record of the recipient lists, %1, is not "
                 "readable. Signed recipient lists are not accepted until "
                 "it is repaired or removed (which forgets what was accepted "
                 "before).")
                 .arg(path);
  }
  return value;
}

/// A record file written through, whole, in place: `format` and @p inner
/// under @p name. Nothing is cached: a failed write leaves the next
/// transaction reading what is on disk.
auto saveFile(const QString &path, const QString &name,
              const QJsonObject &inner, QString *error) -> bool {
  QJsonObject root;
  root.insert(QLatin1String("format"), kRecordFormat);
  root.insert(name, inner);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error)
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists, %1, cannot "
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
      *error = QCoreApplication::translate(
                   "GpgIdGeneration",
                   "The generation record of the recipient lists, %1, cannot "
                   "be written.")
                   .arg(path);
    return false;
  }
  return true;
}

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
  return saveFile(path, QStringLiteral("lists"), lists, error);
}

auto saveStores(const QString &path, const Stores &stores, QString *error)
    -> bool {
  QJsonObject byKey;
  for (auto it = stores.constBegin(); it != stores.constEnd(); ++it) {
    QJsonObject item;
    item.insert(QLatin1String("id"), it.value());
    byKey.insert(it.key(), item);
  }
  return saveFile(path, QStringLiteral("stores"), byKey, error);
}

/// Process mutex, then a lock file against a second QtPass (no single-instance
/// guard, a Flatpak next to a native one), then a fresh read of the record and
/// the store ids. When `ok()` is false (no lock, or a file that does not
/// parse) the caller neither uses nor writes them: RecordUnavailable.
struct Transaction {
  QMutexLocker<QMutex> mutex{&recordLock()};
  QString path{recordPath()};
  // The record's folder may not exist before its first write; the lock file
  // needs it now.
  bool haveFolder{QDir().mkpath(QFileInfo(path).absolutePath())};
  QLockFile lock{path + QStringLiteral(".lock")};
  bool locked{haveFolder && lock.tryLock(kRecordLockTimeoutMs)};
  QString problem;
  QString storesFile{storesPath()};
  std::optional<Record> entries;

  Transaction() {
    if (!locked) {
      problem = QCoreApplication::translate(
                    "GpgIdGeneration",
                    "The generation record of the recipient lists, %1, could "
                    "not be locked: another QtPass may be using it, or its "
                    "folder cannot be written.")
                    .arg(path);
      return;
    }
    std::optional<Entries> lists =
        loadFile<Entries>(path, parseRecord, &problem);
    if (!lists) {
      return;
    }
    std::optional<Stores> stores =
        loadFile<Stores>(storesFile, parseStores, &problem);
    if (!stores) {
      return;
    }
    entries = Record{std::move(*lists), std::move(*stores)};
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
    return entries->lists.value(key).generation;
  }

  /// The store id this device knows the store by, for @p key of its root;
  /// nothing before the first one.
  [[nodiscard]] auto store(const QString &key) const -> std::optional<QString> {
    const auto it = entries->stores.constFind(key);
    if (it == entries->stores.constEnd()) {
      return std::nullopt;
    }
    return it.value();
  }

  /// Remember @p id for the store with @p key, through to disk.
  auto pinStore(const QString &key, const QString &id, QString *error) -> bool {
    Stores updated = entries->stores;
    updated.insert(key, id);
    if (!saveStores(storesFile, updated, error)) {
      return false;
    }
    entries->stores = std::move(updated);
    return true;
  }

  /// The remembered digest for @p key; nothing when none was recorded (a
  /// reservation not yet written, or a record from before digests).
  [[nodiscard]] auto digest(const QString &key) const
      -> std::optional<QString> {
    const QString value = entries->lists.value(key).digest;
    if (value.isEmpty()) {
      return std::nullopt;
    }
    return value;
  }

  /// Write @p generation with @p digest (empty: none) for @p key through
  /// to disk.
  auto remember(const QString &key, qint64 generation, const QString &digest,
                QString *error) -> bool {
    Entries updated = entries->lists;
    updated[key] = Entry{generation, digest};
    if (!saveRecord(path, updated, error)) {
      return false;
    }
    entries->lists = std::move(updated);
    return true;
  }
};

/// Whether @p value holds a control character (a line break, a tab, a NUL,
/// DEL): the folder line can carry none of them, so parse() refuses a line
/// with one and folderOf() a folder whose name has one.
auto hasControl(const QByteArray &value) -> bool {
  return std::any_of(value.cbegin(), value.cend(), [](const char c) {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7f;
  });
}

/// Store-relative, `/`-separated, canonical, `.` for the root; nothing when
/// @p path is not under @p root, or the name holds a control character.
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
  folder = folder.mid(prefix.size());
  // The folder line is one line: a name with a line break in it would end
  // it, and what follows would be read as a recipient of the signed list.
  // Any other control character parse() refuses: the list written would be
  // one that cannot be read back.
  if (hasControl(folder.toUtf8())) {
    return std::nullopt;
  }
  return folder;
}
} // namespace

namespace {

/// The digits of a generation line, or nothing when the line does not hold
/// one ASCII decimal number of at most kMaxDigits digits and nothing else.
auto generationDigits(const QByteArray &digits) -> std::optional<qint64> {
  if (digits.isEmpty() || digits.size() > kMaxDigits) {
    return std::nullopt;
  }
  for (const char c : digits) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
  }
  return digits.toLongLong();
}

/// Takes one line of ours into @p generation or @p folder. Returns why it
/// cannot be taken (a second line of the kind, a malformed one, an unknown
/// kind), or an empty string when it was.
auto takeHeaderLine(const QByteArray &line, std::optional<qint64> &generation,
                    std::optional<QString> &folder,
                    std::optional<QString> &store) -> QString {
  if (line.startsWith(GpgIdGeneration::kStorePrefix)) {
    if (store) {
      return QCoreApplication::translate(
          "GpgIdGeneration", "The list carries more than one store line.");
    }
    const QByteArray value = line.mid(GpgIdGeneration::kStorePrefix.size());
    if (isStoreId(value)) {
      store = QString::fromLatin1(value);
      return {};
    }
    return QCoreApplication::translate("GpgIdGeneration",
                                       "The store line is malformed: %1")
        .arg(QString::fromUtf8(line));
  }
  if (line.startsWith(GpgIdGeneration::kGenerationPrefix)) {
    if (generation) {
      return QCoreApplication::translate(
          "GpgIdGeneration", "The list carries more than one generation line.");
    }
    generation =
        generationDigits(line.mid(GpgIdGeneration::kGenerationPrefix.size()));
    if (generation) {
      return {};
    }
    return QCoreApplication::translate("GpgIdGeneration",
                                       "The generation line is malformed: %1")
        .arg(QString::fromUtf8(line));
  }
  if (line.startsWith(GpgIdGeneration::kFolderPrefix)) {
    if (folder) {
      return QCoreApplication::translate(
          "GpgIdGeneration", "The list carries more than one folder line.");
    }
    const QByteArray value = line.mid(GpgIdGeneration::kFolderPrefix.size());
    // A control character (a second \r of "\r\r\n", a tab, a NUL) is not
    // part of a folder name QtPass writes, and would not survive being
    // written back: header() ends the line, parse() strips one \r.
    if (!value.isEmpty() && !hasControl(value)) {
      folder = QString::fromUtf8(value);
      return {};
    }
    return QCoreApplication::translate("GpgIdGeneration",
                                       "The folder line is malformed: %1")
        .arg(QString::fromUtf8(line));
  }
  return QCoreApplication::translate("GpgIdGeneration",
                                     "The header line is malformed: %1")
      .arg(QString::fromUtf8(line));
}

} // namespace

auto GpgIdGeneration::parse(const QByteArray &contents, QString *error)
    -> std::optional<Header> {
  std::optional<qint64> generation;
  std::optional<QString> folder;
  std::optional<QString> store;
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
    const QString complaint = takeHeaderLine(line, generation, folder, store);
    if (complaint.isEmpty()) {
      continue;
    }
    if (error)
      *error = complaint;
    return std::nullopt;
  }
  if (generation && !folder) {
    if (error)
      *error = tr("The list carries a generation line but no folder line.");
    return std::nullopt;
  }
  // QtPass writes the store line only with the other two.
  if (store && !generation) {
    if (error)
      *error = tr("The list carries a store line but no generation line.");
    return std::nullopt;
  }
  Header header;
  header.generation = generation.value_or(0);
  header.folder = folder;
  header.store = store;
  return header;
}

auto GpgIdGeneration::header(qint64 generation, const QString &folder,
                             const QString &store) -> QByteArray {
  QByteArray lines = kGenerationPrefix + QByteArray::number(generation) + '\n' +
                     kFolderPrefix + folder.toUtf8() + '\n';
  if (!store.isEmpty()) {
    lines += kStorePrefix + store.toLatin1() + '\n';
  }
  return lines;
}

auto GpgIdGeneration::withHeader(qint64 generation, const QString &folder,
                                 const QByteArray &recipients,
                                 const QString &store) -> QByteArray {
  return header(generation, folder, store) + recipients;
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

auto GpgIdGeneration::storeRecordFile() -> QString { return storesPath(); }

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

auto GpgIdGeneration::boundToItsFolder(const QString &gpgIdFile,
                                       const Header &header,
                                       const QString &storeRoot, QString *error)
    -> bool {
  if (!header.folder) {
    return true;
  }
  const std::optional<QString> here = folderOf(gpgIdFile, storeRoot);
  if (here && *here == *header.folder) {
    return true;
  }
  if (error)
    *error =
        tr("The signed recipient list %1 was written for the folder "
           "\"%2\" of the store, not for \"%3\", and is not used. It "
           "may have been copied here by someone else; if the folder "
           "was moved or renamed instead, a holder of the signing key "
           "opens Users on it and saves the recipients, which binds "
           "the list to where it is now.")
            .arg(gpgIdFile, *header.folder, here.value_or(QStringLiteral("?")));
  return false;
}

auto GpgIdGeneration::wayThrough(qint64 last, const QString &recordPath,
                                 const QString &saving) -> QString {
  if (last >= kMaxGeneration) {
    return tr("Generation %1 is the highest there is, so no newer list "
              "can be written here: removing %2 forgets what this device "
              "accepted before, after which a holder of the signing key "
              "gets through by opening Users and %3.")
        .arg(last)
        .arg(recordPath, saving);
  }
  return tr("A holder of the signing key gets through by opening Users "
            "and %1, which writes generation %2. Removing %3 forgets what "
            "this device accepted before.")
      .arg(saving)
      .arg(last + 1)
      .arg(recordPath);
}

auto GpgIdGeneration::staleList(const QString &gpgIdFile, const Header &header,
                                qint64 last, const QString &recordPath,
                                QString *error) -> Verdict {
  if (!header.folder) {
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
              .arg(
                  wayThrough(last, recordPath,
                             tr("selecting the recipients afresh and saving")));
    return Verdict::Unbound;
  }
  if (error)
    *error =
        tr("The signed recipient list %1 is generation %2, older than "
           "generation %3, the last one QtPass accepted here. It may have "
           "been put back by someone else. %4")
            .arg(gpgIdFile)
            .arg(header.generation)
            .arg(last)
            .arg(wayThrough(last, recordPath,
                            tr("saving the recipients: the preselected "
                               "recipients there are this list's, so remove "
                               "anyone who should no longer have access "
                               "first")));
  return Verdict::Rollback;
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
  if (!boundToItsFolder(gpgIdFile, *header, storeRoot, error)) {
    return Verdict::WrongFolder;
  }
  Transaction t;
  if (!t.ok(error)) {
    return Verdict::RecordUnavailable;
  }
  const QString k = key(gpgIdFile);
  const qint64 last = t.generation(k);
  const qint64 generation = header->generation;
  const QString bytes = digest(contents);
  // Before anything that could preselect the list's recipients (a rollback
  // is recovered through the Users dialog): a list of another store is no
  // history of this one.
  const QString storeKey = key(storeRoot);
  const std::optional<QString> pinned = t.store(storeKey);
  if (pinned && !ofThisStore(gpgIdFile, *header, *pinned,
                             generation == last && t.digest(k) == bytes,
                             t.storesFile, error)) {
    return Verdict::WrongStore;
  }
  // First sight of the store's id, on a list that is otherwise accepted.
  const auto pinHere = [&]() {
    if (pinned || !header->store) {
      return true;
    }
    return t.pinStore(storeKey, *header->store, error);
  };
  if (generation < last) {
    return staleList(gpgIdFile, *header, last, t.path, error);
  }
  if (generation == 0) {
    // pass writes headerless lists on every change, so differing bytes here
    // are normal, not a conflict. Generation 0 has no freshness to keep
    // (SECURITY.md).
    return pinHere() ? Verdict::Accepted : Verdict::RecordUnavailable;
  }
  if (generation == last) {
    // Same generation, different bytes: two devices both made 19 from 18
    // (Git's conflict), or a swap. A record without a digest (an unrecorded
    // reservation) takes these bytes as the ones.
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
                     .arg(wayThrough(last, t.path,
                                     tr("checking the recipients and saving")));
      return Verdict::Conflict;
    }
    if (known) {
      return pinHere() ? Verdict::Accepted : Verdict::RecordUnavailable;
    }
  }
  // The pin first: a generation recorded with the pin's write failing would
  // leave the store unpinned for the next list, another store's included.
  if (!pinHere() || !t.remember(k, generation, bytes, error)) {
    return Verdict::RecordUnavailable;
  }
  return Verdict::Accepted;
}

auto GpgIdGeneration::ofThisStore(const QString &gpgIdFile,
                                  const Header &header, const QString &pinned,
                                  bool alreadyAccepted,
                                  const QString &recordPath, QString *error)
    -> bool {
  if (header.store) {
    if (*header.store == pinned) {
      return true;
    }
    if (error)
      *error =
          tr("The signed recipient list %1 was written for another password "
             "store (store ID %2; this device knows this store as %3) and is "
             "not used. It may have been copied here from a store signed "
             "with the same key. If this store was set up again instead, "
             "removing %4 forgets the store IDs this device knows.")
              .arg(gpgIdFile, *header.store, pinned, recordPath);
    return false;
  }
  // No store line: a list without a generation (pass) has none to carry,
  // and one this device accepted before it had a store id still is what it
  // was. Anything new must say which store it is for.
  if (header.generation == 0 || alreadyAccepted) {
    return true;
  }
  if (error)
    *error =
        tr("The signed recipient list %1 carries no store line, while this "
           "device knows this store by its ID %2: it may have been written "
           "for another store and copied here, or by an older QtPass. A "
           "holder of the signing key gets through by opening Users, "
           "selecting the recipients afresh and saving, which writes the "
           "store line.")
            .arg(gpgIdFile, pinned);
  return false;
}

auto GpgIdGeneration::storeIdFor(const QString &storeRoot,
                                 const std::optional<QString> &onDisk,
                                 QString *error) -> std::optional<QString> {
  Transaction t;
  if (!t.ok(error)) {
    return std::nullopt;
  }
  const QString storeKey = key(storeRoot);
  if (const std::optional<QString> pinned = t.store(storeKey)) {
    return pinned;
  }
  // Joining a store that has an id already: take it over, rather than
  // giving the store a second one every other device would refuse.
  const QString id = onDisk && isStoreId(onDisk->toLatin1())
                         ? *onDisk
                         : QUuid::createUuid().toString(QUuid::Id128).toLower();
  if (!t.pinStore(storeKey, id, error)) {
    return std::nullopt;
  }
  return id;
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
