// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements RealPass: builds the `pass` command lines and does itself
 * the few things `pass` must not be trusted with, such as removing a linked
 * folder without following it.
 */
#include "realpass.h"
#include "executor.h"
#include "gpgidsigner.h"
#include "pathvalidator.h"
#include "qtpasslogging.h"
#include "util.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

using Enums::GIT_COMMIT;
using Enums::GIT_INIT;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::PASS_COPY;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;

RealPass::RealPass() : m_grep(this) {
  connect(&m_grep, &NativeGrep::finished, this, &RealPass::finishedGrep);
  connect(&exec, &Executor::dequeuing, this, &RealPass::onDequeuing);
}

RealPass::~RealPass() { m_grep.cancel(); }

void RealPass::GitInit() { executePass(GIT_INIT, {"git", "init"}); }

void RealPass::GitPull_b() {
  // In the store's environment, as every queued pass call: pass picks its
  // store from PASSWORD_STORE_DIR alone, so without it the pull before an
  // edit went to ~/.password-store instead of the configured store.
  QProcess process;
  process.setProcessEnvironment(storeEnvironment());
  QString err;
  const int result =
      Executor::executeBlocking(process, m_settings.passExecutable,
                                {"git", "pull"}, QString(), nullptr, &err);
  if (result != 0) {
    qCDebug(lcQtPass) << "Git pull failed with code:" << result;
    // As the native backend does: the entry about to be edited is not up to
    // date with the remote.
    emit statusMsg(tr("Git pull failed: %1").arg(err.trimmed()), 5000);
  }
}

void RealPass::GitPull() { executePass(GIT_PULL, {"git", "pull"}); }

void RealPass::GitPush() {
  // Queued behind an init, move or copy, the push would run before its
  // outcome is known and send its commit even when the re-encryption then
  // fails (the hold is set only then): it waits for them.
  if (!m_reencryptScopes.isEmpty() || !m_reencryptPending.isEmpty()) {
    m_pushAfterReencrypt = true;
    m_deferredPushByHand = m_deferredPushByHand || !pushIsAutomatic();
    return;
  }
  executePass(GIT_PUSH, {"git", "push"});
}

void RealPass::sendDeferredPush() {
  if (!m_pushAfterReencrypt || !m_reencryptScopes.isEmpty() ||
      !m_reencryptPending.isEmpty()) {
    return;
  }
  const bool byHand = m_deferredPushByHand;
  m_pushAfterReencrypt = false;
  m_deferredPushByHand = false;
  if (!byHand && autoPushHeld()) {
    emit statusMsg(tr("Not pushing: an earlier re-encryption in this "
                      "store did not finish"),
                   5000);
    return;
  }
  executePass(GIT_PUSH, {"git", "push"});
}

void RealPass::Show(QString file) {
  // pass follows links as readily as gpg does.
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  queueShow(file);
  executePass(PASS_SHOW, {"show", file}, "", true);
}

auto RealPass::listSigner() -> GpgIdSigner {
  return {m_settings.gpgExecutable,
          GpgIdSigner::keysFromSetting(m_settings.passSigningKey),
          [this](const QString &app, const QStringList &args,
                 const QString &input, QString *out, QString *err) {
            QProcess gpg;
            gpg.setProcessEnvironment(exec.environment());
            return Executor::executeBlocking(gpg, app, args, input, out, err);
          }};
}

auto RealPass::refuseList(const QString &file, const GpgIdSigner &signer,
                          QSet<QString> &verified, const QString &within,
                          const QString &except) -> bool {
  if (refuseSkippedGpgId(file, true, except)) {
    return true;
  }
  const QString gpgIdFile = Pass::getGpgIdPath(file, m_settings.passStore);
  const QString absolute =
      QDir::cleanPath(QFileInfo(gpgIdFile).absoluteFilePath());
  if (verified.contains(gpgIdFile) ||
      (!except.isEmpty() &&
       absolute == QDir::cleanPath(QFileInfo(except).absoluteFilePath())) ||
      (!within.isEmpty() && !absolute.startsWith(within))) {
    return false;
  }
  QString why =
      QCoreApplication::translate("ImitatePass", "Signature for %1 is invalid.")
          .arg(gpgIdFile);
  QByteArray contents;
  if (!verifySignedList(signer, gpgIdFile, &contents, &why)) {
    emit critical(QCoreApplication::translate("ImitatePass",
                                              "Check .gpg-id file signature!"),
                  why);
    return true;
  }
  verified.insert(gpgIdFile);
  return false;
}

auto RealPass::refuseUnverifiedList(const QString &file) -> bool {
  // pass verifies the signature itself, but knows nothing of the generation
  // and folder lines: an older authentic list put back, or one signed for
  // another folder, would get the secret (#2077). The same checks as the
  // native backend, with gpg in the store's environment. pass reads the list
  // again afterwards; a swap in between is a concurrent writer, out of
  // scope (SECURITY.md).
  const GpgIdSigner signer = listSigner();
  if (!signer.enabled()) {
    return false;
  }
  QSet<QString> verified;
  return refuseList(file, signer, verified);
}

namespace {
/// @p dir and every folder under it as pass's reencrypt_path() walks them
/// (find without -L): hidden ones included, .git left out, no link
/// followed. Each entry pass re-encrypts lies in one of them, and its list
/// is that folder's nearest. A folder holding a name Qt cannot read
/// (Util::hasUnreadableName()) goes to @p unreadable instead of being
/// walked: find sees what Qt leaves out, a folder with its own list
/// included.
auto passFoldersUnder(const QString &dir, QStringList &unreadable)
    -> QStringList {
  QStringList folders{QDir::cleanPath(dir)};
  if (Util::hasUnreadableName(dir)) {
    unreadable << QDir::cleanPath(dir);
    return folders;
  }
  const QFileInfoList children = QDir(dir).entryInfoList(
      QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
  for (const QFileInfo &child : children) {
    if (child.isSymLink() || child.isJunction() ||
        child.fileName() == QLatin1String(".git")) {
      continue;
    }
    folders << passFoldersUnder(child.absoluteFilePath(), unreadable);
  }
  return folders;
}
} // namespace

auto RealPass::refuseUnverifiedTree(const QString &dir, bool onlyListsWithin,
                                    const QString &except) -> bool {
  const GpgIdSigner signer = listSigner();
  if (!signer.enabled()) {
    return false;
  }
  // One gpg call per list, before pass runs: a store with many lists makes
  // that a noticeable pause, so say what it is.
  emit statusMsg(tr("Checking the recipient lists under %1…")
                     .arg(QDir::toNativeSeparators(QDir::cleanPath(dir))),
                 3000);
  // pass mv and cp re-encrypt every entry of the folder they write to, each
  // to its nearest list, walking as passFoldersUnder() does. For the folder
  // that moves, the lists inside it go along: checked where they lie now
  // (after the move their folder line names the old place until the
  // recipients are saved there again), while a list above it is replaced
  // by the destination's, which the caller checks. For entries already in
  // the destination, their lists stay where they are: all of them count.
  // Absolute, as getGpgIdPath() hands lists back: a relative or other
  // spelling would match nothing and check nothing.
  const QString within =
      onlyListsWithin ? QDir::cleanPath(QFileInfo(dir).absoluteFilePath()) +
                            QLatin1Char('/')
                      : QString();
  QSet<QString> verified;
  // Every folder's list counts, also where no entry uses it yet: cp -r
  // merges an incoming folder into one of the same name, so the entries it
  // brings get a list already there, and a list it brings replaces the one
  // there for the entries already in it.
  QStringList unreadable;
  const QStringList folders = passFoldersUnder(dir, unreadable);
  if (!unreadable.isEmpty()) {
    const QString title = tr("Cannot check the recipient lists");
    //: %1 is a folder. "UTF-8" is the text encoding; keep it as is.
    const QString text =
        tr("%1 holds a file or folder whose name is not valid UTF-8. QtPass "
           "cannot see what is behind that name, while pass would re-encrypt "
           "it, so nothing was changed. Rename it, then try again.")
            .arg(QDir::toNativeSeparators(unreadable.first()));
    emit critical(title, text);
    return true;
  }
  for (const QString &folder : folders) {
    if (refuseList(folder, signer, verified, within, except)) {
      return true;
    }
  }
  return false;
}

auto RealPass::storeGit(const QStringList &args, QString *out) -> bool {
  if (!m_settings.useGit) {
    return false;
  }
  // `pass git` runs git in the store with PASSWORD_STORE_DIR set, as the
  // store's other quick lookups do; its exit code is git's own.
  QString err;
  return Executor::executeBlocking(
             exec.environment(), m_settings.passExecutable,
             QStringList{QStringLiteral("git")} + args, out, &err) == 0;
}

namespace {
/**
 * @brief @p args for `pass git`, run in @p repository.
 * @param repository A folder relative to the store; empty for the store.
 * @param args The git arguments.
 * @return `-C <repository>` and @p args.
 */
auto inRepository(const QString &repository, const QStringList &args)
    -> QStringList {
  if (repository.isEmpty()) {
    return args;
  }
  return QStringList{QStringLiteral("-C"), repository} + args;
}
} // namespace

auto RealPass::repositoryOf(const QString &path) -> QString {
  if (!m_settings.useGit) {
    return {};
  }
  const QString root = QDir::cleanPath(m_settings.passStore);
  // From the nearest folder that exists, as pass looks for it.
  QString folder = QDir::cleanPath(path);
  while (!QFileInfo(folder).isDir() &&
         folder.startsWith(root + QLatin1Char('/'))) {
    folder = QFileInfo(folder).path();
  }
  const QString relative = QDir(root).relativeFilePath(folder);
  if (relative == QLatin1String(".") || relative == QLatin1String("..") ||
      relative.startsWith(QLatin1String("../"))) {
    return {};
  }
  // The folder's place in its repository ("team/" in the store's own,
  // "" at the top of the one it holds itself).
  QString prefix;
  storeGit({QStringLiteral("-C"), relative, QStringLiteral("rev-parse"),
            QStringLiteral("--show-prefix")},
           &prefix);
  if (prefix.endsWith(QLatin1Char('\n'))) {
    prefix.chop(1);
  }
  const QString withSlash = relative + QLatin1Char('/');
  if (!withSlash.endsWith(prefix)) {
    return {};
  }
  QString repository = withSlash.chopped(prefix.size());
  if (repository.endsWith(QLatin1Char('/'))) {
    repository.chop(1);
  }
  return repository;
}

auto RealPass::statusEntries(const QStringList &args, const QString &repository)
    -> QStringList {
  if (!m_settings.useGit) {
    return {};
  }
  QString out;
  // -z: names as they are, not quoted when they hold a non-ASCII letter.
  // `pass git` exits 0 whatever git did: the output decides, and a git
  // that cannot say (no repository) prints nothing to it.
  storeGit(inRepository(repository, QStringList{QStringLiteral("status"),
                                                QStringLiteral("--porcelain"),
                                                QStringLiteral("-z")} +
                                        args),
           &out);
  const QString place =
      repository.isEmpty() ? QString() : repository + QLatin1Char('/');
  QStringList entries;
  const QStringList fields = out.split(QChar(0), Qt::SkipEmptyParts);
  for (qsizetype i = 0; i < fields.size(); ++i) {
    const QString &field = fields.at(i);
    if (field.size() < 4 || field.at(2) != QLatin1Char(' ')) {
      continue;
    }
    entries << field.left(3) + place + field.mid(3);
    // A rename or copy, in the index or in the work tree (an intent-to-add
    // file), is followed by its old name.
    if (field.at(0) == QLatin1Char('R') || field.at(0) == QLatin1Char('C') ||
        field.at(1) == QLatin1Char('R') || field.at(1) == QLatin1Char('C')) {
      ++i;
    }
  }
  return entries;
}

auto RealPass::unrecorded(const QStringList &paths, bool ignored)
    -> QStringList {
  if (paths.isEmpty()) {
    return {};
  }
  const QDir store(QDir::cleanPath(m_settings.passStore));
  // Each path in the repository pass commits it in: a folder may hold one
  // of its own, which the store's status does not look into.
  QMap<QString, QStringList> byRepository;
  for (const QString &path : paths) {
    const QString repository = repositoryOf(path);
    const QDir top(repository.isEmpty() ? store.path()
                                        : store.filePath(repository));
    byRepository[repository] << top.relativeFilePath(QDir::cleanPath(path));
  }
  QStringList entries;
  for (auto it = byRepository.cbegin(); it != byRepository.cend(); ++it) {
    QStringList args{QStringLiteral("--untracked-files=all")};
    if (ignored) {
      args << QStringLiteral("--ignored");
    }
    args << QStringLiteral("--");
    entries += statusEntries(args + it.value(), it.key());
  }
  return entries;
}

namespace {
/**
 * @brief The untracked files among @p lines that a file manager writes by
 * itself in the folders it opens: Finder's `.DS_Store`, Explorer's
 * `Thumbs.db` and `desktop.ini`, Dolphin's `.directory`. A rule in a
 * .gitignore keeps those out; one Git already tracks it does not.
 * @param lines `git status --porcelain` entries.
 * @param paths Receives each such file's path, as the entry names it.
 * @return Each such name once, in that order.
 */
auto fileManagerFiles(const QStringList &lines, QStringList *paths)
    -> QStringList {
  QStringList found;
  for (const QString &name :
       {QStringLiteral(".DS_Store"), QStringLiteral("Thumbs.db"),
        QStringLiteral("desktop.ini"), QStringLiteral(".directory")}) {
    for (const QString &line : lines) {
      if (line.startsWith(QLatin1String("?? ")) &&
          QFileInfo(line.mid(3)).fileName() == name) {
        if (!found.contains(name)) {
          found << name;
        }
        *paths << line.mid(3);
      }
    }
  }
  return found;
}

/**
 * @brief Whether @p name is one pass writes: an entry or a recipient list.
 * @param name A file name.
 * @return true for `*.gpg`, `.gpg-id` and `.gpg-id.sig`.
 */
auto isStoreFileName(const QString &name) -> bool {
  return name.endsWith(QLatin1String(".gpg")) ||
         name == QLatin1String(".gpg-id") ||
         name == QLatin1String(".gpg-id.sig");
}

/// At most this many status lines are named in a message.
constexpr qsizetype kMaxNamedChanges = 10;

/**
 * @brief @p lines for a message: the first kMaxNamedChanges, and how many
 * more there are.
 * @param lines `git status --porcelain` lines.
 * @return One per line.
 */
auto namedChanges(const QStringList &lines) -> QString {
  QString named = lines.mid(0, kMaxNamedChanges).join(QLatin1Char('\n'));
  if (lines.size() > kMaxNamedChanges) {
    named += QLatin1Char('\n') +
             RealPass::tr("and %n more", nullptr,
                          static_cast<int>(lines.size() - kMaxNamedChanges));
  }
  return named;
}
} // namespace

auto RealPass::refuseUnrecordedChanges(const QStringList &paths,
                                       const QStringList &committing) -> bool {
  QStringList changes = unrecorded(paths, false);
  QStringList repositories;
  for (const QString &path : committing) {
    const QString repository = repositoryOf(path);
    if (!repositories.contains(repository)) {
      repositories << repository;
    }
  }
  // pass commits the whole index, not only what it staged: anything staged
  // elsewhere goes along.
  for (const QString &repository : std::as_const(repositories)) {
    for (const QString &entry :
         statusEntries({QStringLiteral("--untracked-files=no")}, repository)) {
      if (entry.at(0) != QLatin1Char(' ') && !changes.contains(entry)) {
        changes << entry;
      }
    }
  }
  if (changes.isEmpty()) {
    return false;
  }
  QString why =
      tr("The folder holds changes Git has not recorded, and pass would "
         "commit and push them along with this one:\n\n%1\n\nCommit, "
         "remove or ignore them with Git first, then try again.")
          .arg(namedChanges(changes));
  // These come back in every folder the file manager opens: removing them
  // does not last, ignoring them once does, in the .gitignore of the
  // repository that reports them (a folder may hold one of its own).
  QStringList writtenAt;
  const QStringList written = fileManagerFiles(changes, &writtenAt);
  if (!written.isEmpty()) {
    const QDir store(QDir::cleanPath(m_settings.passStore));
    // Each .gitignore with the names its repository reports, alone: a
    // rule for a name that is not there could hide something else.
    QStringList ignoreFiles;
    QMap<QString, QStringList> namesFor;
    for (const QString &path : std::as_const(writtenAt)) {
      const QString repository = repositoryOf(store.filePath(path));
      const QString file = repository.isEmpty()
                               ? QStringLiteral(".gitignore")
                               : repository + QStringLiteral("/.gitignore");
      if (!ignoreFiles.contains(file)) {
        ignoreFiles << file;
      }
      const QString name = QFileInfo(path).fileName();
      if (!namesFor[file].contains(name)) {
        namesFor[file] << name;
      }
    }
    QStringList blocks;
    for (const QString &file : std::as_const(ignoreFiles)) {
      blocks << file + QStringLiteral(":\n") +
                    namesFor.value(file).join(QLatin1Char('\n'));
    }
    why += QStringLiteral("\n\n") +
           tr("The system writes %1 by itself, in folders a file manager "
              "opens (Finder on macOS, Explorer on Windows, Dolphin on "
              "KDE). Git leaves such files alone in every folder once each "
              ".gitignore in the store below holds its lines, committed:"
              "\n\n%2")
               .arg(written.join(QStringLiteral(", ")),
                    blocks.join(QStringLiteral("\n\n")));
  }
  // As the other refusals: the interface arms when an operation starts and
  // is released by processErrorExit(); a refusal starts nothing.
  emit startingExecuteWrapper();
  emit critical(tr("Changes Git has not recorded"), why);
  emit processErrorExit(1, why);
  return true;
}

void RealPass::reportUnrecorded(const QStringList &paths) {
  QStringList changes;
  for (const QString &line : unrecorded(paths, true)) {
    // What pass writes: entries and recipient lists. Other ignored files in
    // a folder are the user's own business.
    if (isStoreFileName(QFileInfo(line.mid(3)).fileName())) {
      changes << line;
    }
  }
  if (changes.isEmpty()) {
    return;
  }
  emit critical(tr("Saved, not committed"),
                tr("The change is in the store, but Git did not record it, "
                   "so it is not pushed, and a pull may overwrite it "
                   "(\"!!\" marks a file Git ignores):\n\n%1")
                    .arg(namedChanges(changes)));
}

auto RealPass::refuseIgnoredThatMoves(const QString &src, const QString &dest)
    -> bool {
  if (!m_settings.useGit || !QFileInfo(src).isDir()) {
    return false;
  }
  const QDir store(QDir::cleanPath(m_settings.passStore));
  // pass moves a folder into an existing one, as mv does.
  const QString landing = QFileInfo(dest).isDir()
                              ? QDir(dest).filePath(QFileInfo(src).fileName())
                              : dest;
  // Each side in its own repository: where the files lie now, and the one
  // pass commits them in.
  const QString fromRepository = repositoryOf(src);
  const QString toRepository = repositoryOf(landing);
  const QString from =
      QDir(fromRepository.isEmpty() ? store.path()
                                    : store.filePath(fromRepository))
          .relativeFilePath(QDir::cleanPath(src));
  const QString to =
      QDir(toRepository.isEmpty() ? store.path() : store.filePath(toRepository))
          .relativeFilePath(QDir::cleanPath(landing));
  // ls-files names files relative to where git runs, where status names
  // them from the repository's root; both are that repository here.
  QString out;
  storeGit(
      inRepository(fromRepository,
                   {QStringLiteral("ls-files"), QStringLiteral("-z"),
                    QStringLiteral("--others"), QStringLiteral("--ignored"),
                    QStringLiteral("--exclude-standard"), QStringLiteral("--"),
                    from}),
      &out);
  QStringList moving;
  QStringList landed;
  for (const QString &file : out.split(QChar(0), Qt::SkipEmptyParts)) {
    if (!file.startsWith(from + QLatin1Char('/')) ||
        isStoreFileName(QFileInfo(file).fileName())) {
      continue;
    }
    moving << file;
    landed << to + file.mid(from.size());
  }
  if (landed.isEmpty()) {
    return false;
  }
  // A rule in a .gitignore inside the folder goes with it, so what it
  // ignores stays ignored where it lands, and decides there: deeper rules
  // win. Into a folder that is already there, a rule waiting deeper in it
  // could undo that, so only the lookup there counts.
  QStringList travelling;
  QStringList rules;
  if (!QFileInfo::exists(landing) &&
      checkIgnore(fromRepository, moving, true, &rules)) {
    for (qsizetype i = 0; i + 3 < rules.size(); i += 4) {
      if (rules.at(i).startsWith(from + QLatin1Char('/'))) {
        travelling << rules.at(i + 3);
      }
    }
  }
  // A lookup that failed counts nothing as ignored: the move is refused.
  QStringList ignoredThere;
  if (!checkIgnore(toRepository, landed, false, &ignoredThere)) {
    ignoredThere.clear();
  }
  QStringList exposed;
  for (qsizetype i = 0; i < landed.size(); ++i) {
    if (!travelling.contains(moving.at(i)) &&
        !ignoredThere.contains(landed.at(i))) {
      exposed << moving.at(i) + QStringLiteral(" > ") + landed.at(i);
    }
  }
  if (exposed.isEmpty()) {
    return false;
  }
  const QString why =
      tr("Git ignores these files only where they are now, and pass would "
         "commit and push them where they land:\n\n%1\n\nMove them out of "
         "the folder, or have Git ignore them there too, then try again.")
          .arg(namedChanges(exposed));
  emit startingExecuteWrapper();
  emit critical(tr("Ignored files would be committed"), why);
  emit processErrorExit(1, why);
  return true;
}

auto RealPass::checkIgnore(const QString &repository, const QStringList &paths,
                           bool verbose, QStringList *fields) -> bool {
  // check-ignore takes -z only with --stdin. Without
  // GIT_LITERAL_PATHSPECS (below) a name starting with ':' reads as
  // pathspec magic: "./" keeps every name a name. Git answers with the
  // name as given.
  const QString here = QStringLiteral("./");
  QString input;
  for (const QString &path : paths) {
    input += here + path + QChar(0);
  }
  QStringList args{QStringLiteral("check-ignore"), QStringLiteral("--stdin"),
                   QStringLiteral("-z")};
  if (verbose) {
    args << QStringLiteral("-v");
  }
  // check-ignore refuses every path under GIT_LITERAL_PATHSPECS, which
  // the store's environment sets; it takes its paths as names anyway.
  QProcessEnvironment environment = storeEnvironment();
  environment.remove(QStringLiteral("GIT_LITERAL_PATHSPECS"));
  QProcess lookup;
  lookup.setProcessEnvironment(environment);
  QString out;
  QString err;
  (void)Executor::executeBlocking(lookup, m_settings.passExecutable,
                                  QStringList{QStringLiteral("git")} +
                                      inRepository(repository, args),
                                  input, &out, &err);
  // pass git exits 0 whatever git did: a lookup that failed shows only on
  // stderr.
  if (!err.trimmed().isEmpty()) {
    return false;
  }
  *fields = out.split(QChar(0));
  if (!fields->isEmpty() && fields->constLast().isEmpty()) {
    fields->removeLast();
  }
  // The path is every field, or every fourth with -v.
  const qsizetype step = verbose ? 4 : 1;
  for (qsizetype i = step - 1; i < fields->size(); i += step) {
    if ((*fields)[i].startsWith(here)) {
      (*fields)[i].remove(0, here.size());
    }
  }
  return true;
}

void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  const QString entry =
      QDir::cleanPath(QDir::isAbsolutePath(file)
                          ? file + ".gpg"
                          : QDir(m_settings.passStore).filePath(file) + ".gpg");
  if (refuseUnverifiedList(entry)) {
    return;
  }
  if (refuseUnfinishedGitOperation() || refusePendingPull() ||
      refuseUnrecordedChanges({}, {entry})) {
    return;
  }
  m_insertQueue.enqueue(entry);
  // Past every refusal: this insert is now queued and will report once.
  queueInsert();
  QStringList args = {"insert", "-m"};
  if (overwrite) {
    args.append("-f");
  }
  // pass takes every name as one below the store root: an absolute one
  // would be saved under <store>/<the whole path>, for the root's list
  // rather than the one refuseUnverifiedList() checked.
  args.append(passName(entry, true));
  executePass(PASS_INSERT, args, newValue);
}

void RealPass::onDequeuing(int id, bool completes) {
  if (id == PASS_INIT || id == PASS_MOVE || id == PASS_COPY) {
    // Leaves the queue with its command: a dropped one's scope must not
    // pair with the next completion.
    const QString scope =
        m_reencryptScopes.isEmpty() ? QString() : m_reencryptScopes.dequeue();
    if (!completes) {
      return;
    }
    m_reencryptPending.enqueue(scope);
    // pass exits 0 when it could not re-encrypt an entry (reencrypt_path
    // drops the temporary and carries on), so its exit code says nothing.
    // gpg's status lines do, in every locale: on stderr (fd 1 is the
    // `gpg -d | gpg -e` pipe), for this one process (Pass resets the
    // environment before each start), the last --status-fd winning over a
    // user's own; pass's own `--status-fd=1` for verifying a signed
    // .gpg-id comes later on its command line and still wins. WSLENV
    // already forwards PASSWORD_STORE_GPG_OPTS.
    QProcessEnvironment environment = exec.environment();
    const QString key = QStringLiteral("PASSWORD_STORE_GPG_OPTS");
    environment.insert(
        key,
        (environment.value(key) + QStringLiteral(" --status-fd 2")).trimmed());
    exec.setEnvironment(environment);
    return;
  }
  if (id != PASS_INSERT || m_insertQueue.isEmpty()) {
    return;
  }
  const QString target = m_insertQueue.dequeue();
  if (completes) {
    m_insertPending.enqueue({target, entryFingerprint(target)});
  }
}

auto RealPass::insertWritten(const QString &err) const -> bool {
  if (m_insertDone.target.isEmpty()) {
    return false;
  }
  // gpg broke off (a full disk, killed): pass says so, and what gpg left
  // at the entry's name, if anything, is no entry. The line is pass's own
  // (`die "Password encryption aborted."` after gpg in cmd_insert and
  // cmd_edit of password-store.sh), which pass does not translate.
  if (err.contains(QLatin1String("Password encryption aborted."))) {
    return false;
  }
  // pass writes the new ciphertext to the entry's name before it commits,
  // and an encryption never gives the same bytes twice (a fresh session
  // key), so an edited entry has changed as surely as a new one appeared.
  const QByteArray after = entryFingerprint(m_insertDone.target);
  return !after.isEmpty() && after != m_insertDone.before;
}

auto RealPass::entryFingerprint(const QString &path) -> QByteArray {
  QFile file;
  if (!Util::openRegularFile(path, file)) {
    return {};
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    return {};
  }
  return hash.result();
}

void RealPass::Remove(QString file, bool isDir) {
  // Nothing behind a link is the store's to delete.
  if (refuseLinkedPath(isDir ? file : file + ".gpg", LinkScope::ParentsOnly)) {
    return;
  }
  if (refuseUnfinishedGitOperation() || refusePendingPull()) {
    return;
  }
  // pass rm turns a folder into "<folder>/" and rm -rf then follows a link
  // and empties its target, so a linked folder is unlinked here and pass's
  // git is only asked to forget it. pass rm -f on a linked .gpg unlinks.
  const QString full = QDir::cleanPath(m_settings.passStore + file);
  if (isDir && Util::isLinkedFolder(full)) {
    if (!Util::removeTree(full)) {
      const QString why = tr("Could not remove the link %1.").arg(full);
      emit critical(tr("Delete failed"), why);
      emit processErrorExit(1, why);
      return;
    }
    if (m_settings.useGit) {
      // `pass git` exits 0 whatever git did, so ask by output whether git
      // knew the link (these quick lookups block; the commit is queued).
      const QString rel = QDir(m_settings.passStore).relativeFilePath(full);
      // With the store's environment (PASSWORD_STORE_DIR), as executePass.
      QString known;
      QString err;
      // The link is gone from disk either way. A failed lookup or rm leaves
      // git tracking it: say so rather than report a clean removal.
      bool recorded = Executor::executeBlocking(
                          exec.environment(), m_settings.passExecutable,
                          {"git", "ls-files", "--", rel}, &known, &err) == 0;
      if (recorded && !known.trimmed().isEmpty()) {
        recorded =
            Executor::executeBlocking(
                exec.environment(), m_settings.passExecutable,
                {"git", "rm", "-q", "--cached", "--", rel}, &known, &err) == 0;
        if (recorded) {
          // `pass git commit` is git's commit as it is: pass adds -S only
          // to the commits it makes itself, so ask as its git_commit() does.
          QString signs;
          const bool sign =
              Executor::executeBlocking(
                  exec.environment(), m_settings.passExecutable,
                  {"git", "config", "--bool", "--get", "pass.signcommits"},
                  &signs, nullptr) == 0 &&
              signs.trimmed() == QLatin1String("true");
          QStringList commit{"git", "commit", "-q"};
          if (sign) {
            commit << "-S";
          }
          // Queued, as every other commit, so a signer or hook that waits
          // (a pinentry, an agent) does not freeze the window; finished()
          // judges it by what is left staged, since `pass git` exits 0
          // whatever git did.
          m_linkCommits.enqueue(
              {rel, m_settings.passExecutable, exec.environment()});
          executePass(
              GIT_COMMIT,
              commit + QStringList{"-m", "Remove for " + rel + " using QtPass.",
                                   "--", rel});
          return;
        }
      }
      if (!recorded) {
        const QString why =
            tr("The link %1 was removed, but git could not record it: %2")
                .arg(rel, err.trimmed());
        emit critical(tr("Delete incomplete"), why);
        emit processErrorExit(1, why);
      }
    }
    // Nothing ran for pass to finish: the removal is done here, and the
    // store changed.
    emit finishedRemove(QString(), QString());
    return;
  }
  executePass(PASS_REMOVE, {"rm", (isDir ? "-rf" : "-f"), file});
}

void RealPass::finished(int id, int exitCode, const QString &out,
                        const QString &err) {
  if (static_cast<PROCESS>(id) == GIT_COMMIT && !m_linkCommits.isEmpty()) {
    // Whatever the exit code (pass itself failing, or 0 whatever git
    // did), what is left staged decides.
    finishLinkCommit(m_linkCommits.dequeue(), out, err);
    return;
  }
  if (static_cast<PROCESS>(id) == PASS_INSERT) {
    // This completion's own insert, not whichever left the queue last.
    m_insertDone =
        m_insertPending.isEmpty() ? InsertRecord{} : m_insertPending.dequeue();
  }
  const auto pid = static_cast<PROCESS>(id);
  if (pid == PASS_INIT || pid == PASS_MOVE || pid == PASS_COPY) {
    // Without a recorded scope (not queued through Init/Move/Copy), the
    // whole store: held as a whole, released by nothing short of it.
    const QString scope =
        m_reencryptPending.isEmpty() ? QString() : m_reencryptPending.dequeue();
    if (exitCode == 0 && reencryptFailed(out, err)) {
      holdAutoPush(scope);
      sendDeferredPush();
      reportReencryptFailure(pid, out, err);
      return;
    }
    // A run that failed may have stopped after its commit (pass init commits
    // the new .gpg-id before re-encrypting; signing it may fail, the process
    // may be killed): held as well, until a clean init covers it. A move or
    // copy that failed before re-encrypting anything (the destination
    // exists, a mistyped name) changed no recipients: not held.
    if (exitCode != 0 && (pid == PASS_INIT ||
                          out.contains(QLatin1String(": reencrypting to ")))) {
      holdAutoPush(scope);
    }
    // pass init re-encrypts everything under its folder: a clean run
    // releases the holds there. A move or copy covers only its entries.
    if (pid == PASS_INIT && exitCode == 0 && !scope.isEmpty()) {
      releaseAutoPush(scope);
    }
    if (exitCode == 0 && !scope.isEmpty()) {
      reportUnrecorded({scope});
    }
    sendDeferredPush();
    // The status lines are for this check, not for the output panel.
    Pass::finished(id, exitCode, out, withoutStatusLines(err));
    return;
  }
  if (pid == PASS_INSERT && exitCode == 0 && !m_insertDone.target.isEmpty()) {
    reportUnrecorded({m_insertDone.target});
  }
  Pass::finished(id, exitCode, out, err);
}

auto RealPass::reencryptFailed(const QString &out, const QString &err) -> bool {
  // pass's own line for each entry it re-encrypts ("name: reencrypting to
  // KEY..."), which it does not translate; one per line, counted once
  // even when the entry's own name holds the words.
  const QStringList outLines = out.split(QLatin1Char('\n'));
  const auto attempts = std::count_if(
      outLines.cbegin(), outLines.cend(), [](const QString &line) {
        return line.contains(QLatin1String(": reencrypting to "));
      });
  qsizetype encrypted = 0;
  bool status = false;
  for (QString line : err.split(QLatin1Char('\n'))) {
    line.remove(QLatin1Char('\r'));
    if (!line.startsWith(QLatin1String("[GNUPG:] "))) {
      continue;
    }
    status = true;
    // A recipient gpg cannot encrypt to, or a decrypt that failed (no
    // secret key, pinentry cancelled, no smartcard): that entry was left
    // as it was. KEYEXPIRED and NO_SECKEY alone are not failures.
    if (line.startsWith(QLatin1String("[GNUPG:] INV_RECP ")) ||
        line.startsWith(QLatin1String("[GNUPG:] FAILURE ")) ||
        line.startsWith(QLatin1String("[GNUPG:] DECRYPTION_FAILED"))) {
      return true;
    }
    if (line.startsWith(QLatin1String("[GNUPG:] END_ENCRYPTION"))) {
      ++encrypted;
    }
  }
  // Without any status line the option did not reach gpg (a pass that does
  // not pass PASSWORD_STORE_GPG_OPTS on): nothing to judge by.
  return status && encrypted < attempts;
}

void RealPass::reportReencryptFailure(PROCESS pid, const QString &out,
                                      const QString &err) {
  QStringList failures;
  for (QString line : err.split(QLatin1Char('\n'))) {
    line.remove(QLatin1Char('\r'));
    // Encrypt-side only: gpgErrorMessage() reads any FAILURE as a key that
    // cannot encrypt, wrong advice for a decrypt that failed (no secret
    // key, pinentry cancelled), whose reason gpg's own text gives.
    if (line.startsWith(QLatin1String("[GNUPG:] INV_RECP ")) ||
        line.startsWith(QLatin1String("[GNUPG:] FAILURE encrypt"))) {
      failures << line;
    }
  }
  const QString human = withoutStatusLines(err).trimmed();
  const QString reason = gpgErrorMessage(failures.join(QLatin1Char('\n')));
  QString body =
      tr("The recipients were changed, but some entries could not be "
         "re-encrypted for them: those are still encrypted to the previous "
         "recipients, who can still read them. With Git the change is "
         "committed locally and not pushed: QtPass does not push this store "
         "automatically until a re-encryption succeeds, and a push by hand "
         "sends it.");
  for (const QString &part : {reason, human}) {
    if (!part.isEmpty()) {
      body += QStringLiteral("\n\n") + part;
    }
  }
  // pass's "reencrypting to" lines and gpg's own text, for the output panel.
  emit finishedAnyWithPid(out, human, pid);
  emit critical(
      QCoreApplication::translate("ImitatePass", "Re-encryption failed"), body);
  // Not finishedInit/Move/Copy: that would push the store as it now is.
  emit processErrorExit(1, body);
}

void RealPass::finishLinkCommit(const LinkCommit &commit, const QString &out,
                                const QString &err) {
  // In the store the commit ran in, which a profile switch meanwhile does
  // not change.
  const QString &rel = commit.rel;
  QString staged;
  const bool committed =
      Executor::executeBlocking(
          commit.environment, commit.passExecutable,
          {"git", "diff", "--cached", "--name-only", "--", rel}, &staged,
          nullptr) == 0 &&
      staged.trimmed().isEmpty();
  if (committed) {
    // As Pass::finished() would: git's output (a hook's warning, say) for
    // the output panel, then the removal.
    emit finishedAnyWithPid(out, err, GIT_COMMIT);
    emit finishedRemove(out, err);
    return;
  }
  const QString why = tr("The link %1 was removed and git no longer tracks "
                         "it, but the commit failed: %2")
                          .arg(rel, err.trimmed());
  emit critical(tr("Delete incomplete"), why);
  emit processErrorExit(1, why);
  emit finishedRemove(QString(), QString());
}

void RealPass::Init(QString path, const QList<UserInfo> &users) {
  // A relative path names a folder in the store, as --path does for pass:
  // resolved once, so the checks below look where pass writes, not in the
  // working directory.
  const QString folder = QDir::cleanPath(
      QDir::isAbsolutePath(path) ? path
                                 : QDir(m_settings.passStore).filePath(path));
  // pass init writes .gpg-id with a shell redirection and gpg writes the
  // signature with --output; both follow a link planted under those names.
  // --path is store-relative, or pass creates passStore/passStore/dir: the
  // store check hands it over as it judged it, so a store root reached
  // through a link and a folder spelled through its target agree (a lexical
  // relativeFilePath() made that ../../target/dir, which pass refuses).
  QString dirWithoutPassdir;
  const auto refusePlaced = [this](const QString &placed) {
    return refuseLinkedPath(placed, LinkScope::EntryAndParents,
                            PathSpelling::PlacedByCaller);
  };
  if (refusePlaced(folder) ||
      refusePlaced(folder + QStringLiteral("/.gpg-id")) ||
      refusePlaced(folder + QStringLiteral("/.gpg-id.sig")) ||
      refuseFolderOutsideStore(folder, &dirWithoutPassdir)) {
    return;
  }
  // pass init writes this folder's list and then re-encrypts everything
  // under it to each entry's nearest list, a deeper folder's own included:
  // those are checked as for a move. This folder's list is the one being
  // written, so it is neither verified nor missed. Walked in the store's
  // own spelling: a folder spelled through the target of a linked store
  // root is not under the store as named, and the lookup of each nearest
  // list would stop at once and miss the deeper ones.
  const QString inStore =
      QDir::cleanPath(QDir(m_settings.passStore).filePath(dirWithoutPassdir));
  if (refuseUnverifiedTree(inStore, true,
                           QDir(inStore).filePath(QStringLiteral(".gpg-id")))) {
    return;
  }
  if (refuseUnfinishedGitOperation() || refusePendingPull() ||
      refuseUnrecordedChanges({inStore}, {inStore})) {
    return;
  }
  QStringList args = {"init", "--path=" + dirWithoutPassdir};
  for (const UserInfo &user : users) {
    if (user.enabled) {
      args.append(user.key_id);
    }
  }
  m_reencryptScopes.enqueue(inStore);
  executePass(PASS_INIT, args);
}

void RealPass::Move(const QString src, const QString dest, const bool force) {
  passMoveOrCopy(PASS_MOVE, QStringLiteral("mv"), src, dest, force);
}

void RealPass::Copy(const QString src, const QString dest, const bool force) {
  passMoveOrCopy(PASS_COPY, QStringLiteral("cp"), src, dest, force);
}

auto RealPass::passName(const QString &path, bool stripGpg) const -> QString {
  QString name =
      QDir(QDir::cleanPath(m_settings.passStore))
          .relativeFilePath(QDir::cleanPath(QDir(path).absolutePath()));
  if (stripGpg) {
    name.replace(Util::endsWithGpg(), "");
  }
  return name;
}

void RealPass::passMoveOrCopy(PROCESS id, const QString &subcommand,
                              const QString &src, const QString &dest,
                              const bool force) {
  if (refuseLinkedPath(src) || refuseLinkedPath(dest)) {
    return;
  }
  QFileInfo srcFileInfo = QFileInfo(src);
  QFileInfo destFileInfo = QFileInfo(dest);
  // A drop hands over the folder; pass then writes <folder>/<name>, and cp
  // writes through a link planted under that name.
  if (destFileInfo.isDir() &&
      refuseLinkedPath(QDir(dest).filePath(srcFileInfo.fileName()))) {
    return;
  }

  // pass always forces when called from QtPass, so refuse file-onto-file.
  if (!force && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  // pass re-encrypts what it moves or copies to the destination's list, a
  // folder's entries also to the lists that go along with it, and, when it
  // writes into an existing folder, everything already in that folder too
  // (reencrypt_path on the whole destination).
  if (refuseUnverifiedList(destFileInfo.isDir()
                               ? QDir(dest).filePath(srcFileInfo.fileName())
                               : dest) ||
      (srcFileInfo.isDir() && refuseUnverifiedTree(src, true)) ||
      (destFileInfo.isDir() && refuseUnverifiedTree(dest, false))) {
    return;
  }
  // pass stages what it moved or copied with `git add` on the new path,
  // and a folder brings whatever lies in it along. It commits there, and a
  // move commits the removal where the original was as well.
  const QStringList committing =
      id == PASS_MOVE ? QStringList{src, dest} : QStringList{dest};
  if (refuseUnfinishedGitOperation() || refusePendingPull() ||
      refuseUnrecordedChanges({src, dest}, committing) ||
      refuseIgnoredThatMoves(src, dest)) {
    return;
  }

  // pass appends .gpg itself, so an entry named with it would come out as
  // "name.gpg.gpg"; the destination usually does not exist yet.
  const QString passSrc =
      passName(src, srcFileInfo.isFile() &&
                        srcFileInfo.suffix() == QLatin1String("gpg"));
  const QString passDest =
      passName(dest, !destFileInfo.isDir() &&
                         destFileInfo.suffix() == QLatin1String("gpg"));

  QStringList args;
  args << subcommand;
  if (force) {
    args << "-f";
  }
  args << passSrc;
  args << passDest;
  m_reencryptScopes.enqueue(QDir::cleanPath(QDir(dest).absolutePath()));
  executePass(id, args);
}

// Not `pass grep` (find -L follows links out of the store): the native
// search walks real files only (Util::regularFilesUnder), so it needs gpg.
void RealPass::Grep(QString pattern, bool caseInsensitive) {
  if (m_settings.gpgExecutable.isEmpty()) {
    emit statusMsg(tr("Search needs the GPG executable to be configured."),
                   5000);
    emit finishedGrep({});
    return;
  }
  m_grep.search(pattern, caseInsensitive, m_settings.gpgExecutable,
                m_settings.passStore, exec.environment());
}

void RealPass::executePass(PROCESS id, const QStringList &args, QString input,
                           bool readStdout, bool readStderr) {
  executeWrapper(id, m_settings.passExecutable, args, std::move(input),
                 readStdout, readStderr);
}
