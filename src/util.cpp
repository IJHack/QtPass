// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class Util
 * @brief Static utility functions implementation.
 *
 * Implementation of utility functions for path handling, binary discovery,
 * and configuration validation.
 *
 * @see util.h
 */

#include "util.h"
#include "appsettings.h"
#include "executor.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpressionMatchIterator>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>
#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "qtpasslogging.h"

QProcessEnvironment Util::_env;
bool Util::_envInitialised = false;

/**
 * @brief Initializes the process environment and augments PATH with
 * platform-specific GPG locations.
 * @example
 * Util::initialiseEnvironment();
 *
 * @note On macOS, appends common MacGPG2 and /usr/local/bin paths if available.
 * @note On Windows, appends common WinGPG and GnuPG installation paths if
 * available.
 */
void Util::initialiseEnvironment() {
  if (!_envInitialised) {
    _env = QProcessEnvironment::systemEnvironment();
#ifdef __APPLE__
    QString path = _env.value("PATH");
    if (!path.contains("/usr/local/MacGPG2/bin") &&
        QDir("/usr/local/MacGPG2/bin").exists())
      path += ":/usr/local/MacGPG2/bin";
    if (!path.contains("/usr/local/bin"))
      path += ":/usr/local/bin";
    _env.insert("PATH", path);
#endif
#ifdef Q_OS_WIN
    QString path = _env.value("PATH");
    if (!path.contains("C:\\Program Files\\WinGPG\\x86") &&
        QDir("C:\\Program Files\\WinGPG\\x86").exists())
      path += ";C:\\Program Files\\WinGPG\\x86";
    if (!path.contains("C:\\Program Files\\GnuPG\\bin") &&
        QDir("C:\\Program Files\\GnuPG\\bin").exists())
      path += ";C:\\Program Files\\GnuPG\\bin";
    _env.insert("PATH", path);
#endif
    qCDebug(lcQtPass) << _env.value("PATH");
    _envInitialised = true;
  }
}

/**
 * @brief Resolves the path to the password store directory.
 * @details Initializes the environment, checks for the {@code
 * PASSWORD_STORE_DIR} variable, and falls back to a platform-specific default
 * location under the user's home directory.
 * @return QString - Normalized path to the password store folder.
 */
auto Util::findPasswordStore() -> QString {
  QString path;
  initialiseEnvironment();
  if (_env.contains("PASSWORD_STORE_DIR")) {
    path = Util::expandTilde(_env.value("PASSWORD_STORE_DIR"));
  } else {
#ifdef Q_OS_WIN
    path = QDir(QDir::homePath()).filePath("password-store");
#else
    path = QDir(QDir::homePath()).filePath(".password-store");
#endif
  }
  return Util::normalizeFolderPath(QDir::cleanPath(path));
}

/**
 * @brief Expand a leading current-user tilde in a path.
 *
 * Environment variables set in non-shell contexts (systemd units, .desktop
 * entries, quoted shell assignments) skip shell tilde expansion and keep a
 * literal "~". "~username" forms are intentionally not resolved.
 */
auto Util::expandTilde(const QString &path) -> QString {
  if (path == QLatin1String("~")) {
    return QDir::homePath();
  }
  if (path.startsWith(QLatin1String("~/"))) {
    return QDir::homePath() + path.mid(1);
  }
  return path;
}

auto Util::normalizeFolderPath(const QString &path) -> QString {
  QString normalizedPath = path;
  if (!normalizedPath.endsWith('/')) {
    normalizedPath += '/';
  }
  return normalizedPath;
}

/**
 * @brief Finds the absolute path of a binary by searching the PATH environment
 * variable.
 *
 * Splits the (platform-augmented) PATH into directories and delegates to the
 * two-argument overload. On Windows, if no local match is found, it may fall
 * back to a WSL invocation when the binary name is valid and WSL appears to
 * support it.
 *
 * @example
 * QString result = Util::findBinaryInPath("git");
 * // Expected output sample: "/usr/bin/git" or "wsl git"
 *
 * @param QString binary - The name of the binary to locate.
 * @return QString - The absolute path to the binary, or an empty string if not
 * found.
 */
auto Util::findBinaryInPath(const QString &binary) -> QString {
  if (binary.isEmpty()) {
    return {};
  }

  initialiseEnvironment();

  const QStringList dirs =
      _env.value(QStringLiteral("PATH"))
          .split(QDir::listSeparator(), Qt::SkipEmptyParts);
  QString ret;
  if (QDir::fromNativeSeparators(binary).contains(u'/')) {
    // An explicit path is not a PATH search: an absolute path is checked
    // as-is, a relative one is resolved against the PATH directories. The
    // directory-list overload refuses such names, so handle them here.
    if (QDir::isAbsolutePath(binary)) {
      ret = QStandardPaths::findExecutable(binary);
    } else if (!dirs.isEmpty()) {
      ret = QStandardPaths::findExecutable(binary, dirs);
    }
  } else {
    ret = findBinaryInPath(binary, dirs);
  }
#ifdef Q_OS_WIN
  if (ret.isEmpty()) {
    // Cache per-binary WSL lookup result — the wsl --version probe is a
    // blocking subprocess that can run several times per session for
    // missing binaries; once decided, the answer doesn't change at runtime.
    static QHash<QString, QString> wslBinaryCache;
    const bool hasWhitespace =
        std::any_of(binary.cbegin(), binary.cend(),
                    [](const QChar ch) { return ch.isSpace(); });
    if (!hasWhitespace) {
      auto cached = wslBinaryCache.constFind(binary);
      if (cached != wslBinaryCache.constEnd()) {
        ret = cached.value();
      } else {
        QString wslCommand = QStringLiteral("wsl ") + binary;
        qCDebug(lcQtPass)
            << "Util::findBinaryInPath(): falling back to WSL for binary"
            << binary;
        QString out, err;
        QString cachedResult;
        if (Executor::executeBlocking(wslCommand, {"--version"}, &out, &err) ==
                0 &&
            !out.isEmpty() && err.isEmpty()) {
          qCDebug(lcQtPass)
              << "Util::findBinaryInPath(): using WSL binary" << wslCommand;
          cachedResult = wslCommand;
        }
        wslBinaryCache.insert(binary, cachedResult);
        ret = cachedResult;
      }
    }
  }
#endif

  return ret;
}

/**
 * @brief Finds an executable in an explicit list of directories.
 *
 * Thin wrapper around QStandardPaths::findExecutable(): only regular files
 * that are executable match (a directory named like the binary is skipped),
 * and on Windows the PATHEXT extensions are tried. Empty entries are dropped
 * rather than being resolved against the current working directory, and an
 * empty list finds nothing instead of silently falling back to the process
 * PATH. Only bare names are accepted: QStandardPaths::findExecutable() would
 * return an absolute @p binary without consulting @p searchPaths at all, and
 * a relative one containing ".." could escape them, so both find nothing.
 *
 * @param binary The name of the binary to locate; must not contain a
 * directory separator.
 * @param searchPaths Directories to search, in order.
 * @return QString - The absolute path to the binary, or an empty string if not
 * found.
 */
auto Util::findBinaryInPath(const QString &binary,
                            const QStringList &searchPaths) -> QString {
  if (binary.isEmpty() || QDir::fromNativeSeparators(binary).contains(u'/')) {
    return {};
  }
  QStringList dirs;
  dirs.reserve(searchPaths.size());
  for (const QString &dir : searchPaths) {
    if (!dir.isEmpty()) {
      dirs.append(dir);
    }
  }
  if (dirs.isEmpty()) {
    // QStandardPaths::findExecutable() treats an empty list as "use PATH".
    return {};
  }
  return QStandardPaths::findExecutable(binary, dirs);
}

/**
 * @brief Checks whether the current QtPass configuration is valid.
 * @example
 * AppSettings s = QtPassSettings::load();
 * bool result = Util::configIsValid(s);
 * std::cout << std::boolalpha << result << std::endl; // Expected output: true
 * or false
 *
 * @param s Application settings snapshot to validate.
 * @return bool - True if the configuration file exists and the required
 * executable is available; otherwise false.
 */
auto Util::configIsValid(const AppSettings &s) -> bool {
  const QString configFilePath = QDir(s.passStore).filePath(".gpg-id");
  if (!QFile(configFilePath).exists()) {
    return false;
  }

  const QString executable = s.usePass ? s.passExecutable : s.gpgExecutable;

  if (const auto wsl = Executor::parseWslCommand(executable)) {
    // Probe WSL once per session — availability doesn't change at runtime
    // and the executeBlocking call is a blocking subprocess.
    static const bool wslAvailable = [&wsl]() {
      QString out;
      QString err;
      return Executor::executeBlocking(wsl->launcher,
                                       {QStringLiteral("--version")}, &out,
                                       &err) == 0 &&
             !out.isEmpty() && err.isEmpty();
    }();
    if (wslAvailable) {
      return true;
    }
  }
  return QFile(executable).exists();
}

/**
 * @brief Returns a regex matching strings that end with the .gpg extension.
 *
 * @return QRegularExpression reference
 */
auto Util::endsWithGpg() -> const QRegularExpression & {
  static const QRegularExpression expr{R"(\.gpg$)"};
  return expr;
}

/**
 * @brief Returns a regex matching common remote/network protocol schemes.
 *
 * Matches http://, https://, ftp://, ftps://, ssh://, sftp://, webdav://,
 * webdavs://
 *
 * The URL text ends at the first whitespace character (space, tab, CR, LF),
 * quote or bracket, so a URL on its own line in multi-line text (pass file
 * bodies, gpg stderr) is captured without the line break that follows it.
 *
 * Note: Local file URLs (file:///) are intentionally excluded by design, as
 * they represent local paths rather than network protocols. If this behavior
 * needs to change, update both this function and the corresponding test.
 *
 * @return QRegularExpression reference
 */
auto Util::protocolRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{
      R"(((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://[^"\s<>\)\]\[]+))"};
  return regex;
}

/**
 * @brief Validate a value as a launchable http(s) URL.
 *
 * Security gate for the "open in browser" action. See util.h for the full
 * contract. Deliberately stricter than protocolRegex(): only http/https,
 * valid host, no embedded credentials, no control characters.
 *
 * @param value Candidate URL string.
 * @return true if launchable in a browser, false otherwise.
 */
auto Util::isLaunchableWebUrl(const QString &value) -> bool {
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  // Reject control characters first, before QUrl normalisation can hide a
  // CR/LF/NUL injection into the OS URL handler.
  for (const QChar &c : trimmed) {
    if (c == QLatin1Char('\r') || c == QLatin1Char('\n') ||
        c == QChar(QChar::Null)) {
      return false;
    }
  }
  const QUrl url(trimmed, QUrl::StrictMode);
  if (!url.isValid()) {
    return false;
  }
  const QString scheme = url.scheme().toLower();
  if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
    return false;
  }
  if (url.host().isEmpty()) {
    return false;
  }
  // Embedded userinfo (user:pass@host) would leak into browser history.
  if (!url.userName().isEmpty() || !url.password().isEmpty()) {
    return false;
  }
  return true;
}

/**
 * @brief Escape text as HTML and link only launchable http(s) URLs.
 *
 * See util.h for the contract. Detection uses protocolRegex() so that the
 * URL text is delimited the same way everywhere; the decision whether a
 * match becomes an anchor is isLaunchableWebUrl(), the same predicate that
 * gates the "open in browser" button.
 *
 * @param text Plain text, not yet HTML-escaped.
 * @param linked Set to true when at least one anchor was emitted.
 * @return HTML string safe to hand to QTextBrowser::setHtml().
 */
auto Util::linkifyUrls(const QString &text, bool *linked) -> QString {
  if (linked != nullptr) {
    *linked = false;
  }
  QString html;
  html.reserve(text.size());
  qsizetype lastIndex = 0;
  QRegularExpressionMatchIterator it = protocolRegex().globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString url = match.captured(0);
    if (!isLaunchableWebUrl(url)) {
      // Not a web URL (or it carries credentials): leave it in the escaped
      // plain-text run instead of making it clickable.
      continue;
    }
    const qsizetype start = match.capturedStart(0);
    html += text.mid(lastIndex, start - lastIndex).toHtmlEscaped();
    const QString escapedUrl = url.toHtmlEscaped();
    html += QStringLiteral("<a href=\"%1\">%1</a>").arg(escapedUrl);
    lastIndex = match.capturedEnd(0);
    if (linked != nullptr) {
      *linked = true;
    }
  }
  html += text.mid(lastIndex).toHtmlEscaped();
  return html;
}

/**
 * @brief Returns a regex matching newline characters (CR or LF).
 *
 * Useful for detecting or sanitising line breaks in text content.
 *
 * @return QRegularExpression reference
 */
auto Util::newLinesRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{"[\r\n]"};
  return regex;
}

/**
 * @brief Validate whether a string is an accepted GPG key identifier.
 *
 * Mirrors what `pass` itself accepts in `.gpg-id`: every non-empty token is
 * handed to gpg as a `-r` argument, and gpg resolves it — key ID or
 * fingerprint of any version (v4 hex, v6 hex, with or without `0x`),
 * `<email>`, `=Exact User ID`, a plain name substring, or a `@`/`/`/`#`/`&`
 * routing prefix. No content heuristics are applied here: they can only
 * reject recipients gpg would have accepted, and a rejected line is not just
 * skipped but erased the next time `.gpg-id` is rewritten.
 *
 * The one thing rejected is a token starting with `-`: the recipient list is
 * also passed positionally to `gpg --list-keys`, where such a token would be
 * parsed as an option instead of a key selector.
 *
 * Empty input is invalid.
 *
 * @param keyId Input key identifier string to validate.
 * @return true unless the input is empty or starts with `-`.
 */
auto Util::isValidKeyId(const QString &keyId) -> bool {
  return !keyId.isEmpty() && !keyId.startsWith('-');
}

namespace {
/**
 * @brief Walk @p dir's real, visible directories in sorted pre-order and hand
 * every entry to @p visit before any recursion; a directory is entered only
 * when @p visit returns true for it.
 */
template <typename Visit> void walkStore(const QString &dir, Visit visit) {
  // Absolute, so the results are whatever the caller's cwd; not canonical,
  // so a linked root keeps the name it was configured under.
  QStringList pending{QDir(QDir::cleanPath(dir)).absolutePath()};
  while (!pending.isEmpty()) {
    const QDir current(pending.takeLast());
    // Every entry once, links and hidden entries included, so the decision
    // what to do with each is taken here, before any recursion. QDir::System
    // keeps dangling links in the listing.
    const QFileInfoList entries =
        current.entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden |
                                  QDir::System | QDir::NoDotAndDotDot,
                              QDir::Name);
    QStringList subdirs;
    for (const QFileInfo &entry : entries) {
      if (visit(entry)) {
        subdirs << entry.filePath();
      }
    }
    // Pushed last-to-first so the next one taken is the first by name.
    for (auto it = subdirs.crbegin(); it != subdirs.crend(); ++it) {
      pending << *it;
    }
  }
}

/// A symlink or an NTFS junction: never entered, never listed.
auto isLink(const QFileInfo &entry) -> bool {
  return entry.isSymLink() || entry.isJunction();
}

/// Hidden by attribute, or by the dot convention on every platform: Qt 6.11
/// on macOS answers isHidden() from the UF_HIDDEN flag alone once an entry
/// was lstat()ed (qfilesystemengine_unix.cpp marks the attribute known there
/// without the dot check), and Windows does not consider .stversions hidden
/// at all.
auto isHiddenEntry(const QFileInfo &entry) -> bool {
  return entry.isHidden() || entry.fileName().startsWith(QLatin1Char('.'));
}

/// A real directory that is part of the store: .git, .stversions,
/// .Trash-1000 are not, as with QDirIterator without QDir::Hidden.
auto isStoreDirectory(const QFileInfo &entry) -> bool {
  return !isLink(entry) && entry.isDir() && !isHiddenEntry(entry);
}
} // namespace

auto Util::regularFilesUnder(const QString &dir, const QStringList &nameFilters,
                             QStringList *skipped, bool hiddenFiles)
    -> QStringList {
  QStringList files;
  walkStore(dir, [&](const QFileInfo &entry) {
    if (isStoreDirectory(entry)) {
      return true;
    }
    const bool named = QDir::match(nameFilters, entry.fileName());
    if (isLink(entry) || (!entry.isDir() && !entry.isFile())) {
      // A linked directory hides everything behind it; a linked file, or a
      // FIFO, socket or device, is only of interest under a name the caller
      // asked for.
      if (skipped != nullptr && (entry.isDir() || named)) {
        qCWarning(lcQtPass) << "Skipping" << entry.filePath()
                            << ": not a regular file or directory";
        *skipped << entry.filePath();
      }
      return false;
    }
    if (entry.isFile() && named && (hiddenFiles || !isHiddenEntry(entry))) {
      files << entry.filePath();
    }
    return false;
  });
  return files;
}

auto Util::directoriesUnder(const QString &dir) -> QStringList {
  QStringList dirs;
  walkStore(dir, [&](const QFileInfo &entry) {
    if (!isStoreDirectory(entry)) {
      return false;
    }
    dirs << entry.filePath();
    return true;
  });
  return dirs;
}

auto Util::isLinkedFolder(const QString &path) -> bool {
  return isLink(QFileInfo(QDir::cleanPath(path)));
}

auto Util::isUnderLink(const QString &path, const QString &storeRoot,
                       bool includeSelf) -> bool {
  // Names compare the way the platform's file system compares them:
  // "C:/Store" and "c:/store" are one directory on Windows, and a path
  // spelled the other way must not skip the walk (Pass::getGpgIdPath does
  // the same).
#ifdef Q_OS_WIN
  constexpr auto cs = Qt::CaseInsensitive;
#else
  constexpr auto cs = Qt::CaseSensitive;
#endif
  const QString root = QDir::cleanPath(storeRoot);
  // A root of "/" or "C:/" already ends in the separator.
  const QString prefix =
      root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
  const auto isRoot = [&](const QString &p) {
    return p.compare(root, cs) == 0;
  };
  QString current = QDir::cleanPath(path);
  if (!current.startsWith(prefix, cs)) {
    // Not under the store as named: only the entry itself can be judged.
    return includeSelf && !isRoot(current) && isLinkedFolder(current);
  }
  if (!includeSelf) {
    current = QFileInfo(current).path();
  }
  for (; !isRoot(current) && current.startsWith(prefix, cs);
       current = QFileInfo(current).path()) {
    if (isLinkedFolder(current)) {
      return true;
    }
  }
  return false;
}

auto Util::replaceFile(const QString &from, const QString &to, bool replace)
    -> bool {
#ifdef Q_OS_WIN
  const std::wstring source =
      QDir::toNativeSeparators(QFileInfo(from).absoluteFilePath())
          .toStdWString();
  const std::wstring target =
      QDir::toNativeSeparators(QFileInfo(to).absoluteFilePath()).toStdWString();
  // WRITE_THROUGH: the new entry is on the device when this returns.
  return MoveFileExW(source.c_str(), target.c_str(),
                     (replace ? MOVEFILE_REPLACE_EXISTING : 0) |
                         MOVEFILE_WRITE_THROUGH) != 0;
#else
  const QByteArray source = QFile::encodeName(from);
  const QByteArray target = QFile::encodeName(to);
  if (replace) {
    if (::rename(source.constData(), target.constData()) != 0) {
      return false;
    }
  } else {
    // link() makes no second name where one exists and follows nothing.
    if (::link(source.constData(), target.constData()) != 0) {
      return false;
    }
    ::unlink(source.constData());
  }
  // The directory entry too, so a crash right after does not lose the new
  // name; best effort, as the rename itself has happened.
  const int dir = ::open(QFile::encodeName(QFileInfo(to).path()).constData(),
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dir >= 0) {
    ::fsync(dir);
    ::close(dir);
  }
  return true;
#endif
}

auto Util::openRegularFile(const QString &path, QFile &file) -> bool {
#ifdef Q_OS_WIN
  // FILE_FLAG_OPEN_REPARSE_POINT opens a symbolic link or junction itself
  // rather than its target, so the handle's attributes say what the name
  // was at the moment of the open.
  const std::wstring native =
      QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath())
          .toStdWString();
  HANDLE handle = CreateFileW(
      native.c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  BY_HANDLE_FILE_INFORMATION info{};
  if (!GetFileInformationByHandle(handle, &info) ||
      (info.dwFileAttributes &
       (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY |
        FILE_ATTRIBUTE_DEVICE)) != 0 ||
      GetFileType(handle) != FILE_TYPE_DISK) {
    CloseHandle(handle);
    return false;
  }
  const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                 _O_RDONLY | _O_BINARY);
  if (fd < 0) {
    CloseHandle(handle);
    return false;
  }
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    _close(fd);
    return false;
  }
  return true;
#else
  // O_NOFOLLOW fails with ELOOP on a symbolic link; O_NONBLOCK keeps a FIFO
  // from blocking the open until fstat() can refuse it.
  const int fd = ::open(QFile::encodeName(path).constData(),
                        O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  struct stat st{};
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    ::close(fd);
    return false;
  }
  // The descriptor, not the name, is what QFile opens here: the object
  // fstat() judged is the object read. CodeQL's check-then-use pattern
  // matcher sees an open after a check and cannot tell.
  if (!file.open(fd, QIODevice::ReadOnly, // codeql[cpp/toctou-race-condition]
                 QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    return false;
  }
  return true;
#endif
}

auto Util::syncToDisk(QFileDevice &file) -> bool {
  if (!file.flush()) {
    return false;
  }
#ifdef Q_OS_WIN
  const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(file.handle()));
  return handle != INVALID_HANDLE_VALUE && FlushFileBuffers(handle) != 0;
#else
  return ::fsync(file.handle()) == 0;
#endif
}

auto Util::writeFileReplacing(const QString &path, const QByteArray &bytes,
                              bool replace, QString *error) -> bool {
  QString stagedPath;
  QString why;
  {
    // The QTemporaryFile goes out of scope before the rename: it keeps its
    // handle open for as long as it lives, also after close(), and Windows
    // does not rename an open file.
    QTemporaryFile staged(QFileInfo(path).path() +
                          QStringLiteral("/.qtpass-XXXXXX.tmp"));
    staged.setAutoRemove(false);
    if (!staged.open()) {
      if (error)
        *error = QCoreApplication::translate(
                     "Util", "Cannot create a temporary file next to %1: %2")
                     .arg(path, staged.errorString());
      return false;
    }
    stagedPath = staged.fileName();
    // Owner-only: a .gpg-id names the keys a store is encrypted to, an
    // entry is an entry. QTemporaryFile creates 0600 already; say so for
    // platforms where it may not.
    staged.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    if (staged.write(bytes) != bytes.size() || !syncToDisk(staged)) {
      why = staged.errorString();
    }
  }
  if (!why.isEmpty()) {
    QFile::remove(stagedPath);
    if (error)
      *error = QCoreApplication::translate("Util", "Cannot write %1: %2")
                   .arg(path, why);
    return false;
  }
  if (!replaceFile(stagedPath, path, replace)) {
    QFile::remove(stagedPath);
    if (error) {
      const QFileInfo taken(path);
      if (replace) {
        *error = QCoreApplication::translate("Util", "Failed to replace %1.")
                     .arg(path);
      } else if (taken.exists() || taken.isSymLink()) {
        *error =
            QCoreApplication::translate("Util", "%1 already exists.").arg(path);
      } else {
        *error = QCoreApplication::translate("Util", "Failed to write %1.")
                     .arg(path);
      }
    }
    return false;
  }
  if (QFileInfo(path).isSymLink()) {
    // The temporary's name was swapped for a link before the rename; the
    // bytes went into an unnamed inode, nothing through the link.
    if (error)
      *error = QCoreApplication::translate(
                   "Util", "%1 was replaced by a link while it was written.")
                   .arg(path);
    return false;
  }
  return true;
}

auto Util::removeTree(const QString &dir) -> bool {
  // A trailing separator makes lstat follow a link ("link/" is the target
  // directory); the link itself is what this is about.
  const QString path = QDir::cleanPath(dir);
  const QFileInfo top(path);
  if (isLink(top)) {
    // rm -rf on a link removes the link.
    return QFile::remove(path) || QDir().rmdir(path);
  }
  if (!top.isDir()) {
    return false;
  }
  bool ok = true;
  const QFileInfoList entries =
      QDir(path).entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden |
                                   QDir::System | QDir::NoDotAndDotDot,
                               QDir::Name);
  for (const QFileInfo &entry : entries) {
    const QString entryPath = entry.filePath();
    if (isLink(entry)) {
      // The entry itself, never the target. A junction or a directory
      // symlink on Windows is a directory entry and goes with rmdir.
      if (!QFile::remove(entryPath) && !QDir().rmdir(entryPath)) {
        qCWarning(lcQtPass) << "Could not remove link" << entryPath;
        ok = false;
      }
    } else if (entry.isDir()) {
      ok = removeTree(entryPath) && ok;
    } else if (!QFile::remove(entryPath)) {
      // A read-only file blocks deletion on Windows; give it write access
      // and try once more, as QDir::removeRecursively() does.
      const QFile::Permissions perms = QFile::permissions(entryPath);
      if (perms.testFlag(QFile::WriteUser) ||
          !QFile::setPermissions(entryPath, perms | QFile::WriteUser) ||
          !QFile::remove(entryPath)) {
        qCWarning(lcQtPass) << "Could not remove" << entryPath;
        ok = false;
      }
    }
  }
  return ok && QDir().rmdir(path);
}
