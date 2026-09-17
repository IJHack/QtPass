// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "appsettings.h"
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

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

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
