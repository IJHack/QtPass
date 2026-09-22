// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "appsettings.h"
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <functional>

class QFile;
class QFileDevice;

constexpr int MS_PER_SECOND = 1000;

/*!
    \class Util
    \brief Some static utilities to be used elsewhere.
 */
class Util {
public:
  /**
   * @brief Locate an executable by searching the process PATH and (on Windows)
   * falling back to WSL.
   * @param binary Executable name or relative path to locate (e.g., "gpg" or
   * "pass").
   * @return QString Absolute path to the executable if found, empty QString
   * otherwise.
   */
  static auto findBinaryInPath(const QString &binary) -> QString;
  /**
   * @brief Locate an executable in an explicit list of directories.
   *
   * Only regular, executable files match; a directory named like the binary
   * is skipped. Empty entries are ignored (they never resolve to the current
   * working directory) and an empty list finds nothing. Only bare names are
   * searched: an absolute path, or a name containing a directory separator,
   * finds nothing rather than escaping @p searchPaths.
   * @param binary Executable name to locate (no directory separators).
   * @param searchPaths Directories to search, in order.
   * @return QString Absolute path to the executable if found, empty QString
   * otherwise.
   */
  static auto findBinaryInPath(const QString &binary,
                               const QStringList &searchPaths) -> QString;
  /**
   * @brief Locate the password store directory.
   * @return QString Path to the password store, always ends with '/'.
   */
  static auto findPasswordStore() -> QString;
  /**
   * @brief Expand a leading current-user tilde in a path.
   *
   * Environment variables set outside a shell (systemd units, .desktop
   * entries, quoted assignments) keep a literal "~", which no filesystem call
   * resolves. "~" and "~/..." become the home directory; "~user" forms and
   * everything else are returned unchanged.
   *
   * @param path Path that may start with a tilde.
   * @return QString Path with a leading current-user tilde expanded.
   */
  static auto expandTilde(const QString &path) -> QString;
  /**
   * @brief Ensure a folder path always ends with '/'.
   *
   * Qt normalises paths to forward slashes internally, so this function
   * appends '/' unconditionally rather than the platform-native separator.
   * Callers that need native separators can call QDir::toNativeSeparators()
   * themselves.
   *
   * @param path The folder path to normalize.
   * @return QString Path with a trailing '/' added if it was missing.
   */
  static auto normalizeFolderPath(const QString &path) -> QString;
  /**
   * @brief Verify that the required configuration is complete.
   * @param s Application settings snapshot (passStore, usePass, executables).
   * @return bool `true` if the password store's `.gpg-id` exists AND the
   * configured executable (pass or gpg, depending on settings) exists or is a
   * WSL wrapper; `false` otherwise.
   */
  static auto configIsValid(const AppSettings &s) -> bool;
  /**
   * @brief Returns a regex to match .gpg file extensions.
   * @return Reference to static regex
   */
  static auto endsWithGpg() -> const QRegularExpression &;
  /**
   * @brief Returns a regex to match URL protocols.
   * @return Reference to static regex
   */
  static auto protocolRegex() -> const QRegularExpression &;
  /**
   * @brief Check whether a value is a safe, launchable web URL.
   *
   * Stricter than protocolRegex(): intended to gate an "open in browser"
   * action, where launching a non-web scheme would be a security risk.
   * Returns true only when the trimmed value:
   * - contains no control characters (CR, LF, NUL),
   * - parses to a valid QUrl,
   * - has scheme exactly "http" or "https" (case-insensitive),
   * - has a non-empty host,
   * - carries no embedded userinfo (user:password\@host).
   *
   * Deliberately rejects file://, javascript:, data:, ftp/ssh/webdav and
   * scheme-less inputs (e.g. "www.example.com").
   *
   * @param value Candidate URL string (typically a password-file field).
   * @return true if the value is a launchable http(s) URL, false otherwise.
   */
  static auto isLaunchableWebUrl(const QString &value) -> bool;
  /**
   * @brief Turn plain text into HTML with clickable links for safe web URLs.
   *
   * The single rule for every place a value is rendered into an
   * `<a href>`: the text is HTML-escaped, and only the protocolRegex()
   * matches that also pass isLaunchableWebUrl() become anchors. Everything
   * else (ssh://, ftp://, URLs carrying user:pass\@ credentials, ...) stays
   * escaped plain text, so a click can never hand a non-web scheme or a
   * secret to the OS URL handler via QTextBrowser::setOpenExternalLinks().
   *
   * @param text   Plain (unescaped) text, possibly containing URLs.
   * @param linked Optional out parameter, set to true when at least one
   *               anchor was produced.
   * @return HTML-escaped text with anchors for launchable http(s) URLs.
   */
  static auto linkifyUrls(const QString &text, bool *linked = nullptr)
      -> QString;
  /**
   * @brief Returns a regex to match newline characters.
   * @return Reference to static regex
   */
  static auto newLinesRegex() -> const QRegularExpression &;
  /**
   * @brief Check whether a `.gpg-id` token may be passed to gpg as a key
   * selector.
   * Accepts any non-empty token that does not start with `-`, exactly as
   * `pass` does: key IDs and fingerprints of every version, emails, `=Exact
   * User ID`, plain name substrings and the @, /, #, & routing prefixes.
   * @param keyId The string to validate.
   * @return true if gpg may be given the token, false otherwise.
   */
  static auto isValidKeyId(const QString &keyId) -> bool;
  /**
   * @brief Regular files under @p dir whose name matches one of
   * @p nameFilters, found by walking only real directories.
   *
   * Symbolic links and NTFS junctions are never entered and never listed:
   * either can point outside the tree, and QDirIterator's own recursion
   * treats a junction as an ordinary directory (Qt marks it JunctionType,
   * not LinkType, so QDir::NoSymLinks does not see it). Hidden directories
   * (`.git`, a sync tool's version folder) are not entered either, as with
   * QDirIterator without QDir::Hidden.
   *
   * @p dir itself is followed whatever it is: the configured store root is
   * commonly a symlink (`~/.password-store` to a synced folder), and it is
   * the user's choice, not something found inside the store. A caller that
   * gets its folder from the store tree decides with isLinkedFolder() first.
   *
   * The walk is a listing, not a lock: what is a regular file when listed
   * can be replaced before it is opened. That is the store's trust boundary
   * (SECURITY.md), not something a walk can close.
   * @param dir Directory to walk.
   * @param nameFilters Wildcards, as for QDir::setNameFilters().
   * @param skipped Receives what was left out although it is in the way: every
   *        linked directory, and every link or special file (FIFO, socket,
   *        device) whose name matches @p nameFilters. Null to not care.
   * @param hiddenFiles List hidden files too (`.gpg-id`); hidden directories
   *        stay closed regardless.
   * @return Absolute paths, sorted by name within each directory, a
   *         directory's files before its subdirectories.
   */
  static auto regularFilesUnder(const QString &dir,
                                const QStringList &nameFilters,
                                QStringList *skipped = nullptr,
                                bool hiddenFiles = false) -> QStringList;
  /**
   * @brief Every real, visible directory under @p dir, walked by the same
   * rules as regularFilesUnder(): links, junctions and hidden directories
   * are neither listed nor entered.
   * @param dir Directory to walk.
   * @return Absolute paths, parents before children.
   */
  static auto directoriesUnder(const QString &dir) -> QStringList;
  /**
   * @brief Whether @p path names a symbolic link or NTFS junction, with or
   * without a trailing separator (which would otherwise make the check look
   * at the target instead). Folder or file: a linked `.gpg`, `.gpg-id` or
   * `.gpg-id.sig` is judged the same way.
   *
   * The store tree shows linked entries like any other; an operation on one
   * (show, edit, re-encrypt, recipients, delete) must not reach through it.
   * @param path Path as the tree or a caller names it.
   * @return true for a link or junction.
   */
  static auto isLinkedFolder(const QString &path) -> bool;
  /**
   * @brief Whether the way from @p storeRoot down to @p path passes through
   * a symbolic link or NTFS junction: @p path itself or any folder between.
   *
   * The tree shows a linked folder's children too, and a pick of one of
   * them names a place outside the store as much as the link does. The root
   * itself is not checked: a linked store root is the user's setup.
   * @param path File or folder under the store, as the tree names it.
   * @param storeRoot The configured store.
   * @param includeSelf Whether @p path itself counts; false for a removal,
   *        which unlinks a link and touches nothing behind it.
   * @return true when something on the way is a link.
   */
  static auto isUnderLink(const QString &path, const QString &storeRoot,
                          bool includeSelf = true) -> bool;
  /**
   * @brief Remove @p dir and everything in it, the way `rm -rf` does: a
   * symbolic link or NTFS junction, @p dir itself or anything inside, is
   * removed as an entry and what it points to is never entered. A trailing
   * separator on @p dir does not change that.
   *
   * QDir::removeRecursively() descends into junctions (it stops only at what
   * Qt calls a symlink) and would empty the junction's target.
   * @param dir Directory to remove.
   * @return true when @p dir is gone.
   */
  static auto removeTree(const QString &dir) -> bool;

  /**
   * @brief Give the file at @p from the name @p to, atomically and without
   * following anything: the operating system's rename (`rename(2)`,
   * `MoveFileEx`), never QFile::rename(), whose fallback when the plain
   * rename fails is to copy by opening @p to for writing, through whatever
   * link sits under that name by then. With @p replace, whatever entry is
   * at @p to (a file, a link) is replaced as an entry; without it, the
   * call fails when anything is there (`linkat(2)` without
   * `AT_SYMLINK_FOLLOW` and unlink on POSIX, where a no-replace rename is
   * not portable; plain `link(2)` follows a symlink at @p from on macOS and
   * the BSDs).
   * @param from An existing regular file, on the same filesystem as @p to.
   * @param to The name to give it.
   * @param replace Whether an existing entry at @p to may go.
   * @return Whether @p to is now that file.
   */
  static auto replaceFile(const QString &from, const QString &to, bool replace)
      -> bool;

  /**
   * @brief Open @p path for reading as the regular file it is, not as what a
   * link under that name points to: the object opened is the object judged.
   * A check of the name before an open by name leaves a window in which a
   * co-writer of the store makes the name a link; opening without following
   * (O_NOFOLLOW; on Windows the reparse point itself) and judging the open
   * handle closes it. A directory, a link, a FIFO or a device is refused.
   * @param path The file, as the caller names it.
   * @param file Receives the open file on success; untouched otherwise.
   * @return Whether @p file is open on a regular file.
   */
  static auto openRegularFile(const QString &path, QFile &file) -> bool;

  /**
   * @brief Fills the temporary of stageFileReplacing() through its open
   * handle; returns why it could not (translated, for the user), or an
   * empty string.
   */
  using Filler = std::function<QString(QFileDevice &)>;

  /**
   * @brief The staged write every file QtPass puts into a store goes
   * through: a temporary created next to @p path (an opaque name,
   * exclusive, owner-only), filled by @p fill through its open handle,
   * synced, then replaceFile()d under the name, and the object under the
   * name afterwards checked to be the very file that was filled (opened
   * without following, compared by device and inode). The name is never
   * opened for writing, so a link a co-writer plants under it between the
   * caller's check and the write is replaced as an entry rather than
   * written through (QSaveFile resolves such a link at open); a file swapped
   * under the temporary's name before the rename (a hard link to something
   * of the user's, say) is caught by the comparison and reported, and what
   * is under the name is left (removing it by name could take another
   * writer's file that landed there since). Where the system cannot give
   * both identities the write stands: on FAT and exFAT a rename moves the
   * file ID, and those filesystems have no hard links to swap in either.
   * @param path The file to write.
   * @param replace Whether an existing entry under the name may go.
   * @param fill Writes the contents.
   * @param error Receives why not, if not null.
   * @return Whether @p path now holds what @p fill wrote.
   */
  static auto stageFileReplacing(const QString &path, bool replace,
                                 const Filler &fill, QString *error = nullptr)
      -> bool;

  /**
   * @brief Write @p bytes as the file @p path through stageFileReplacing().
   * @param path The file to write.
   * @param bytes Its contents.
   * @param replace Whether an existing entry under the name may go.
   * @param error Receives why not, if not null.
   * @return Whether @p path now holds @p bytes.
   */
  static auto writeFileReplacing(const QString &path, const QByteArray &bytes,
                                 bool replace, QString *error = nullptr)
      -> bool;

  /**
   * @brief Copy the regular file @p src to the name @p dst through
   * stageFileReplacing(): @p src is read through a handle opened without
   * following (openRegularFile()). The copy is owner-only whatever @p src's
   * mode, as pass's `cp` under its umask 077 makes it.
   * @param src The file to copy; a link, a directory or a special file under
   * that name is refused.
   * @param dst The name to give the copy.
   * @param replace Whether an existing entry under @p dst may go.
   * @param error Receives why not, if not null.
   * @return Whether @p dst now holds a copy of @p src.
   */
  static auto copyFileReplacing(const QString &src, const QString &dst,
                                bool replace, QString *error = nullptr) -> bool;

  /**
   * @brief Flush @p file's bytes to the device (`fsync`, `FlushFileBuffers`)
   * before it is renamed into place, so that a crash right after the rename
   * does not leave the name pointing at an empty file. QSaveFile did this
   * on commit; the staged writes here do it themselves.
   * @param file An open file.
   * @return Whether the data is on the device.
   */
  static auto syncToDisk(QFileDevice &file) -> bool;

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
