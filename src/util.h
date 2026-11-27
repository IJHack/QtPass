// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Util: static helpers for the process environment, binary discovery,
 * store paths, URL safety and the link-safe file operations every write into
 * a store goes through.
 */
#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "appsettings.h"
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QFile;
class QFileDevice;

/// Milliseconds in a second, for timers set from a setting in seconds.
constexpr int MS_PER_SECOND = 1000;

/**
 * @class Util
 * @ingroup support
 * @brief Static helpers that have no better home: setting up the process
 * and the Git environment the way pass does, finding binaries and the
 * default store, path and URL handling, and file operations that refuse to
 * follow links.
 *
 * Nothing here keeps state apart from the cached process environment, and
 * nothing depends on a widget or a model. Store-boundary checks live in
 * PathValidator, SSH agent discovery in SshAuthSock and template files in
 * TemplateIO.
 */
class Util {
public:
  /**
   * @brief Set up the process the way pass sets up its own, before anything
   * runs git or gpg or writes into the store.
   *
   * Clears the variables that point git at another repository (GIT_DIR,
   * GIT_WORK_TREE and the rest pass clears), and on Unix gives the processes
   * QtPass starts (git, gpg, pass) the umask from PASSWORD_STORE_UMASK, 077
   * by default (Executor::setChildUmask()): what they create in the store is
   * then the user's alone, as with pass. QtPass's own umask stays the
   * session's, so a browser or file manager it opens is not affected. Call
   * it first thing in main().
   */
  static void applyPassProcessDefaults();

  /**
   * @brief GIT_CEILING_DIRECTORIES for a store, as pass sets it.
   *
   * The store's parent, so git never takes a repository above the store for
   * the store's own. git compares ceilings with its real working directory,
   * so this is the real parent when the store path is a link; a store that
   * does not exist yet gets the parent of its absolute path.
   * @param store The store's path.
   * @return The ceiling, empty for an empty @p store.
   */
  static auto gitCeiling(const QString &store) -> QString;

  /**
   * @brief The repository above @p store that `pass` itself would commit
   * to, if any.
   *
   * pass fences git in with GIT_CEILING_DIRECTORIES="$PREFIX/..", spelled
   * as the store path is given, and unsets GIT_DIR and GIT_WORK_TREE. git
   * splits that list on ":", so a colon anywhere in the path (above the
   * store or in its own name) leaves no ceiling at the parent; a piece can
   * still be a ceiling further up, which git keeps to, and so does this. A
   * store without its own `.git` then finds a repository above it,
   * and pass adds and commits there on every change, whether or not QtPass
   * uses Git. QtPass cannot fence pass in from outside, so this is for a
   * warning. Always empty on Windows, where pass runs in WSL.
   * @param store The store's path, as PASSWORD_STORE_DIR gets it.
   * @return The folder holding that repository's `.git`; empty when pass
   * keeps to the store.
   */
  static auto repositoryPassClimbsTo(const QString &store) -> QString;

  /**
   * @brief The variables that keep git in @p store's own repository, never
   * one above it: GIT_CEILING_DIRECTORIES (gitCeiling()), and, when that
   * ceiling holds git's list separator (":", ";" on Windows) and so cannot
   * be one entry of the list, GIT_DIR and GIT_WORK_TREE naming the store's
   * own `.git` and the store. A store without a `.git` then has no
   * repository to git, rather than the one above it.
   *
   * An explicit GIT_DIR skips git's ownership check, which runs only when
   * git discovers a repository. On Unix the store is therefore pinned only
   * when git's check would pass: it, its `.git` and a gitfile's target are
   * the user's own, or git's `safe.directory` names the store (as git
   * reads it: only from system, global and command-line configuration,
   * never a repository's own; `*`, an exact path, or a path ending in a
   * slash and `*` for what lies below it, an empty value clearing the
   * entries before it). Otherwise GIT_DIR names
   * a path that cannot exist, and git refuses rather than run another
   * user's repository and hooks.
   * @param store The store's path.
   * @param git The git executable that reads `safe.directory`; "git" from
   *        PATH when empty. Run only for a store that is not the user's own.
   * @param asUser The user to compare owners with; the effective user when
   *        not given (a test passes another).
   * @return Name and value pairs to set; empty for an empty @p store.
   */
  static auto gitStoreVariables(const QString &store, const QString &git = {},
                                std::optional<uint> asUser = std::nullopt)
      -> QList<std::pair<QString, QString>>;
  /**
   * @brief Make git in @p env refuse a folder that is merely laid out as a
   * bare repository (`safe.bareRepository=explicit`, Git 2.38 or newer;
   * older Git ignores it).
   *
   * Someone who can push to a shared store can push such a folder with a
   * config that names a command (`core.fsmonitor`); git started in it would
   * read that config and run the command. The setting goes in through
   * `GIT_CONFIG_*`, command-line scope, which git honours for this protected
   * key and a repository's own config cannot override. It is appended at the
   * next free index, so a user's own entries are kept, and only when the
   * last entry for the key does not already say `explicit`: the last one
   * wins, so a weaker value from the environment is overridden.
   * @param env The environment to amend.
   */
  static void requireExplicitBareRepository(QProcessEnvironment &env);

  /**
   * @brief For tests: the user gitStoreVariables() compares owners with
   * when no @p asUser is given, in place of the effective user. A file
   * owned by another user cannot be made without root.
   * @param user The user, or nothing to go back to the effective user.
   */
  static void setGitOwnerForTests(std::optional<uint> user);

  /**
   * @brief For tests: called by stageFileReplacing() with the temporary's
   * path after it is filled and before it is renamed into place, where a
   * co-writer could swap another file in under that name.
   * @param hook The callback, or an empty one to remove it.
   */
  static void
  setBeforePlacingForTests(std::function<void(const QString &)> hook);

  /**
   * @brief Create @p path and any missing parents as pass would.
   *
   * What pass's mkdir -p does under its umask, for the folders QtPass makes
   * in the store itself: on Unix the mode is 0777 less the store umask
   * (Executor::childUmask(), from PASSWORD_STORE_UMASK, 077 by default and
   * when none is set), whatever QtPass's own umask. Existing folders are
   * left as they are.
   * @param path The folder to create.
   * @return True when @p path is a folder afterwards.
   */
  static auto mkpathForStore(const QString &path) -> bool;

  /**
   * @brief The umask pass would use for @p value of PASSWORD_STORE_UMASK.
   * @param value The variable's value; empty when unset.
   * @return The octal value when it is one (up to four octal digits), else
   *         077, pass's default.
   */
  static auto passUmask(const QString &value) -> int;

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
   * - has an authority (host and port) of at most 259 characters, checked
   *   before QUrl normalises the host,
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
  /** @brief Longest run of white space boundSpaceRuns() leaves. */
  static constexpr qsizetype kMaxSpaceRun = 1000;
  /**
   * @brief @p text with every run of white space other than tabs and line
   * feeds cut to kMaxSpaceRun characters, for showing it with its white
   * space kept.
   *
   * Qt's line layout never finishes a wrapped line with 65536 or more spaces
   * in a row (seen on Qt 6.11 and 6.12), so one crafted entry would freeze
   * the window. No text needs a longer run to read right.
   *
   * @param text Plain text to show.
   * @return @p text, with each longer run cut.
   */
  [[nodiscard]] static auto boundSpaceRuns(const QString &text) -> QString;
  /** @brief Longest name shownName() leaves whole. */
  static constexpr qsizetype kMaxShownName = 120;
  /**
   * @brief A name or path from the store, as a label or list shows it: one
   * line, at most @p max characters.
   *
   * A label is as wide as its longest line and as tall as its lines, and
   * its window grows with it: a co-writer's field name, folder path or
   * template name of tens of thousands of characters, or of thousands of
   * line breaks, made windows hundreds of thousands of pixels large and
   * took gigabytes. Line breaks and other controls show as U+FFFD
   * (displayable()), a tab as a space, and a longer name is cut in the
   * middle with an ellipsis, never between the halves of a surrogate pair.
   * The full name stays where it is used; this is only what is shown.
   * @param name The name or path.
   * @param max Most characters to show.
   * @return The name as shown.
   */
  [[nodiscard]] static auto shownName(const QString &name,
                                      qsizetype max = kMaxShownName) -> QString;
  /**
   * @brief @p text as a tooltip that shows it as written.
   *
   * Qt guesses whether a tooltip is rich text, and as rich text a name a
   * co-writer chose, such as `<img src=/dev/zero>`, would have Qt read
   * that file without end. Escaped, in a paragraph that keeps its white
   * space and still wraps, it is always shown as the text it is,
   * with controls as U+FFFD (displayable()) and long runs of white space
   * cut (boundSpaceRuns()).
   * @param text Plain text, such as a store name or path.
   * @return The tooltip.
   */
  [[nodiscard]] static auto plainToolTip(const QString &text) -> QString;
  /** @brief Longest run without white space hasLongUnbrokenRun() allows. */
  static constexpr qsizetype kMaxUnbrokenRun = 10000;
  /**
   * @brief Whether @p text holds more than kMaxUnbrokenRun characters in a
   * row without white space Qt wraps at (a no-break space is not).
   *
   * Qt wraps such a run at word boundaries in time quadratic in its length:
   * one line of 400 000 letters in a small compressed entry froze the window
   * for seconds, more for longer ones. A widget showing such text wraps
   * anywhere instead (QTextOption::WrapAnywhere), which is linear; no prose
   * has a word this long.
   * @param text Text to show.
   * @return true when such a run is in it.
   */
  [[nodiscard]] static auto hasLongUnbrokenRun(QStringView text) -> bool;
  /**
   * @brief Whether Qt's text document starts a new block at @p c, besides
   * at a line feed: a carriage return, U+2029 (paragraph separator) and the
   * frame marks U+FDD0 and U+FDD1.
   *
   * A limit on lines that counts line feeds only misses these, and a text
   * of millions of blocks freezes or exhausts a text widget.
   * @param c A character.
   * @return true for a block break other than a line feed.
   */
  [[nodiscard]] static auto isOtherBlockBreak(QChar c) -> bool;
  /**
   * @brief @p text with every block break other than a line feed
   * (isOtherBlockBreak()) shown as a space, for a widget whose lines are
   * counted by line feeds.
   * @param text Plain text to show.
   * @return The text with those characters replaced.
   */
  [[nodiscard]] static auto withoutOtherBlockBreaks(QString text) -> QString;
  /**
   * @brief @p text with the characters that change how the text around them
   * reads replaced by U+FFFD, for showing text that comes from the store or
   * a key (a folder name, a key's user ID) as plain text.
   *
   * Replaced: C0 controls (but tab and, with @p lineBreaks, line feed and
   * a carriage return right before one), DEL,
   * C1 controls (NEL among them, U+009C ends a "string" in some terminals),
   * the line and paragraph separators U+2028 and U+2029, and the bidi
   * embedding, override and isolate controls (U+202A to U+202E, U+2066 to
   * U+2069). An override mirrored the rest of a warning; a line separator in
   * a user ID drew a fake fingerprint line. The left-to-right and
   * right-to-left marks stay: right-to-left names use them as they should.
   * @param text Text to show.
   * @param lineBreaks Keep line feeds (a message of several lines).
   * @return @p text with those characters shown as U+FFFD.
   */
  [[nodiscard]] static auto displayable(const QString &text,
                                        bool lineBreaks = true) -> QString;
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
   * @param unlisted Receives every directory the walk could not read (no
   *        read permission, or a name in it that is not valid UTF-8, see
   *        hasUnreadableName()): what is in it is missing from the result,
   *        not absent. Null to not care.
   * @return Absolute paths, sorted by name within each directory, a
   *         directory's files before its subdirectories.
   */
  static auto regularFilesUnder(const QString &dir,
                                const QStringList &nameFilters,
                                QStringList *skipped = nullptr,
                                bool hiddenFiles = false,
                                QStringList *unlisted = nullptr) -> QStringList;
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
   * @brief Whether @p dir holds an entry whose name is not valid UTF-8.
   *
   * Qt reads every file name on Unix as UTF-8 and leaves out an entry whose
   * name is not (reported to end the listing there on some versions), while
   * pass, find and git take names as bytes. A walk over such a folder misses
   * something they still see. Windows names are UTF-16 and always readable.
   * @param dir An existing directory.
   * @return true when a listing of @p dir is incomplete for that reason.
   */
  static auto hasUnreadableName(const QString &dir) -> bool;
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
   * On Windows any reparse point that names another file (a symbolic link,
   * a junction) is refused; another one, such as the placeholder of a cloud
   * provider like OneDrive, is opened through so its content is fetched,
   * when that open reaches the same file (same volume and 128-bit file ID).
   * @param path The file, as the caller names it.
   * @param file Receives the open file on success; untouched otherwise.
   * @return Whether @p file is open on a regular file.
   */
  static auto openRegularFile(const QString &path, QFile &file) -> bool;

  /**
   * @brief Most bytes of a .gpg-id QtPass reads.
   *
   * A recipient list is a few lines; a shared store's other writers could
   * otherwise hand it a sparse file of gigabytes to read before the
   * signature is even checked. Far beyond any real list.
   */
  static constexpr qint64 kMaxGpgIdBytes = 1024 * 1024;

  /**
   * @brief Read @p path whole, as the regular file it is (openRegularFile()),
   * when it holds at most @p maxBytes.
   * @param path The file.
   * @param maxBytes The most bytes accepted.
   * @return The contents; nothing when the file is missing, not a regular
   *         file (a link, FIFO or device included), unreadable or larger.
   */
  static auto readRegularFile(const QString &path, qint64 maxBytes)
      -> std::optional<QByteArray>;

  /**
   * @brief readRegularFile() for a file under @p root, with no link followed
   * anywhere on the way down from it.
   *
   * readRegularFile() refuses a link only as the last part of the name, and
   * a check of the folders before it (isUnderLink()) leaves a window in
   * which a pull makes one of them a link. Here each folder is opened
   * relative to the one above it without following a link (`openat()` with
   * `O_NOFOLLOW | O_DIRECTORY`) and the file relative to the last, so the
   * folders judged are the folders walked. @p root itself may be a link: a
   * linked store root is the user's setup. On Windows each folder is opened
   * by name, refused when it is a symbolic link or junction (what the walk
   * leaves out; another reparse tag, a cloud placeholder, is not), and held
   * open without
   * FILE_SHARE_DELETE until the file is read, so none can be renamed or
   * replaced meanwhile.
   * @param root The store root.
   * @param path The file, spelled under @p root.
   * @param maxBytes The most bytes accepted.
   * @return The contents; nothing when @p path is not under @p root, a part
   *         of the way is a link or not a folder, or readRegularFile() would
   *         refuse the file.
   */
  static auto readRegularFileUnder(const QString &root, const QString &path,
                                   qint64 maxBytes)
      -> std::optional<QByteArray>;

  /**
   * @brief readRegularFileUnder() for a file of the store at @p store,
   *        spelled under the store as configured or under its canonical
   *        path (the target of a linked store root).
   * @param store The store root as configured.
   * @param path The file.
   * @param maxBytes The most bytes accepted.
   * @return The contents; nothing when @p path is under neither spelling
   *         of the store, or readRegularFileUnder() refuses it.
   */
  static auto readRegularFileInStore(const QString &store, const QString &path,
                                     qint64 maxBytes)
      -> std::optional<QByteArray>;

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
   * On POSIX the temporary stays open until the comparison, so its inode
   * number cannot be given to a file swapped in meanwhile (ext4 reuses the
   * number of an unlinked, closed file at once).
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
  /**
   * @brief Fill _env once: the process environment with the usual GPG
   * install directories appended to PATH (MacGPG2 and `/usr/local/bin` on
   * macOS, WinGPG and GnuPG on Windows) when they exist.
   */
  static void initialiseEnvironment();
  /// The environment findBinaryInPath() and findPasswordStore() read, as
  /// initialiseEnvironment() built it.
  static QProcessEnvironment _env;
  /// Whether initialiseEnvironment() has run.
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
