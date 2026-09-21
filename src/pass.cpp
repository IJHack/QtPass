// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "pass.h"
#include "gpgidgeneration.h"
#include "gpgidsigner.h"
#include "gpgkeystate.h"
#include "util.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <utility>

#include "qtpasslogging.h"

using Enums::GIT_INIT;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::GPG_GENKEYS;
using Enums::PASS_COPY;
using Enums::PASS_GREP;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;

namespace {
/**
 * @brief Returns a non-empty charset value, using a fallback when needed.
 * @param input Preferred charset value.
 * @param fallback Charset to use when @p input is empty.
 * @return @p input if it is not empty; otherwise @p fallback.
 */
auto fallbackCharset(const QString &input, const QString &fallback) -> QString {
  return input.isEmpty() ? fallback : input;
}

/**
 * @brief Resolve the effective password character set from configuration.
 *
 * Uses the selected charset index from @p passConfig when it is within range;
 * otherwise falls back to the ALLCHARS entry. If the resolved charset string
 * is empty, falls back again to the ALLCHARS value.
 *
 * @param passConfig Password generation configuration.
 * @return Non-empty charset string to use for password generation.
 */
auto effectiveCharset(const PasswordConfiguration &passConfig) -> QString {
  int sel = passConfig.selected;
  if (sel < 0 || sel >= PasswordConfiguration::CHARSETS_COUNT)
    sel = PasswordConfiguration::ALLCHARS;
  return fallbackCharset(
      passConfig.Characters[sel],
      passConfig.Characters[PasswordConfiguration::ALLCHARS]);
}
} // namespace

/**
 * @brief Pass::Pass wrapper for using either pass or the pass imitation
 */
Pass::Pass() : env(QProcessEnvironment::systemEnvironment()) {
  connect(&exec, &Executor::finished, this, &Pass::finished);
  connect(&exec, &Executor::error, this, &Pass::finished);

  connect(&exec, &Executor::starting, this, &Pass::startingExecuteWrapper);
  // Merge our vars into WSLENV rather than blindly appending a duplicate entry
  const QStringList wslenvVars = {
      QStringLiteral("PASSWORD_STORE_DIR/p"),
      QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH/w"),
      QStringLiteral("PASSWORD_STORE_CHARACTER_SET/w")};
  const QString existing = env.value(QStringLiteral("WSLENV"));
  if (existing.isEmpty()) {
    env.insert(QStringLiteral("WSLENV"), wslenvVars.join(':'));
  } else {
    QStringList parts = existing.split(':', Qt::SkipEmptyParts);
    for (const QString &v : wslenvVars) {
      if (!parts.contains(v))
        parts.append(v);
    }
    env.insert(QStringLiteral("WSLENV"), parts.join(':'));
  }
}

/**
 * @brief Executes a wrapper command.
 * @param id Process ID
 * @param app Application to execute
 * @param args Arguments
 * @param readStdout Whether to read stdout
 * @param readStderr Whether to read stderr
 */
void Pass::executeWrapper(PROCESS id, const QString &app,
                          const QStringList &args, bool readStdout,
                          bool readStderr) {
  executeWrapper(id, app, args, QString(), readStdout, readStderr);
}

void Pass::executeWrapper(PROCESS id, const QString &app,
                          const QStringList &args, QString input,
                          bool readStdout, bool readStderr) {
  beforeExecute(id);
  qCDebug(lcQtPass) << app << loggableArgs(args);
  exec.execute(id, m_settings.passStore, app, args, std::move(input),
               readStdout, readStderr);
}

auto Pass::loggableArgs(const QStringList &args) -> QStringList {
  // Secrets travel on stdin (Insert's content, KeygenDialog's batch file),
  // never in argv; this keeps the log honest should that ever change.
  static const QStringList secretOptions{
      QStringLiteral("--passphrase"), QStringLiteral("--override-session-key")};
  QStringList out;
  bool hide = false;
  for (const QString &arg : args) {
    if (hide) {
      out << QStringLiteral("<redacted>");
      hide = false;
      continue;
    }
    if (arg.startsWith(QStringLiteral("otpauth://"))) {
      out << QStringLiteral("<redacted>");
      continue;
    }
    if (arg.startsWith(QStringLiteral("--passphrase=")) ||
        arg.startsWith(QStringLiteral("--override-session-key="))) {
      out << arg.left(arg.indexOf(u'=') + 1) + QStringLiteral("<redacted>");
      continue;
    }
    hide = secretOptions.contains(arg);
    out << arg;
  }
  return out;
}

void Pass::beforeExecute(PROCESS /*id*/) {}

/**
 * @brief Initializes the pass wrapper with a settings snapshot.
 * @param settings Application settings to use for this backend lifetime.
 */
void Pass::init(const AppSettings &settings) {
  m_settings = settings;
#ifdef __APPLE__
  // If it exists, prepend gpgtools to PATH
  if (QFile(QStringLiteral("/usr/local/MacGPG2/bin")).exists())
    env.insert(QStringLiteral("PATH"),
               QStringLiteral("/usr/local/MacGPG2/bin:") +
                   env.value(QStringLiteral("PATH")));
  // Add missing /usr/local/bin (exact component match, no leading colon)
  const QString currentPath = env.value(QStringLiteral("PATH"));
  if (!currentPath.split(':', Qt::SkipEmptyParts)
           .contains(QStringLiteral("/usr/local/bin"))) {
    env.insert(QStringLiteral("PATH"),
               currentPath.isEmpty()
                   ? QStringLiteral("/usr/local/bin")
                   : QStringLiteral("/usr/local/bin:") + currentPath);
  }
#endif

  // GNUPGHOME: the configured gpgHome wins over the inherited environment,
  // but only when it exists. A gpgHome that is gone (the 1.7.0 test suite
  // left its temporary keyring path in the live QtPass.conf, #1711) would
  // make every gpg call fail with "No secret key"; fall back to whatever the
  // environment says and tell the user. Clearing the setting at runtime
  // restores the inherited value as well instead of keeping the old path.
  const QString inheritedHome = QProcessEnvironment::systemEnvironment().value(
      QStringLiteral("GNUPGHOME"));
  const auto useInheritedHome = [this, &inheritedHome]() {
    if (inheritedHome.isEmpty()) {
      env.remove(QStringLiteral("GNUPGHOME"));
    } else {
      env.insert(QStringLiteral("GNUPGHOME"), inheritedHome);
    }
  };
  if (m_settings.gpgHome.isEmpty()) {
    useInheritedHome();
  } else {
    QDir absHome(m_settings.gpgHome);
    absHome.makeAbsolute();
    if (absHome.exists()) {
      env.insert(QStringLiteral("GNUPGHOME"), absHome.path());
    } else {
      if (inheritedHome.isEmpty()) {
        qCWarning(lcQtPass) << "gpgHome" << absHome.path()
                            << "does not exist; using the default GnuPG home";
        emit statusMsg(tr("Configured GPG home %1 does not exist, using the "
                          "default keyring")
                           .arg(absHome.path()),
                       5000);
      } else {
        qCWarning(lcQtPass)
            << "gpgHome" << absHome.path() << "does not exist; using GNUPGHOME"
            << inheritedHome << "from the environment";
        emit statusMsg(tr("Configured GPG home %1 does not exist, using "
                          "GNUPGHOME %2 from the environment")
                           .arg(absHome.path(), inheritedHome),
                       5000);
      }
      useInheritedHome();
    }
  }
}

/**
 * @brief Pass::Generate use either pwgen or internal password
 * generator
 * @param length of the desired password
 * @param charset to use for generation
 * @return the password
 */
auto Pass::generatePassword(unsigned int length, const QString &charset)
    -> QString {
  if (length == 0) {
    emit critical(tr("Invalid password length"),
                  tr("Can't generate password with zero length."));
    return {};
  }
  QString passwd;
  if (m_settings.usePwgen) {
    // --secure goes first as it overrides --no-* otherwise
    QStringList args;
    args.append("-1");
    if (!m_settings.lessRandom) {
      args.append("--secure");
    }
    args.append(m_settings.avoidCapitals ? "--no-capitalize" : "--capitalize");
    args.append(m_settings.avoidNumbers ? "--no-numerals" : "--numerals");
    if (m_settings.useSymbols) {
      args.append("--symbols");
    }
    args.append(QString::number(length));
    // executeBlocking returns 0 on success, non-zero on failure
    if (Executor::executeBlocking(m_settings.pwgenExecutable, args, &passwd) ==
        0) {
      static const QRegularExpression literalNewLines{"[\\n\\r]"};
      passwd.remove(literalNewLines);
    } else {
      passwd.clear();
      qCDebug(lcQtPass) << "pwgen fail";
      // Error is already handled by clearing passwd; no need for critical
      // signal here
    }
  } else {
    // Validate charset - if CUSTOM is selected but chars are empty,
    // fall back to ALLCHARS to prevent weak passwords (issue #780)
    const QString cs = fallbackCharset(
        charset, m_settings.passwordConfiguration
                     .Characters[PasswordConfiguration::ALLCHARS]);
    if (cs.length() > 0) {
      passwd = generateRandomPassword(cs, length);
    } else {
      emit critical(
          tr("No characters chosen"),
          tr("Can't generate password, there are no characters to choose from "
             "set in the configuration!"));
    }
  }
  return passwd;
}

/**
 * @brief Pass::gpgSupportsEd25519 check if GPG supports ed25519 (ECC)
 * GPG 2.1+ supports ed25519 which is much faster for key generation
 * @return true if ed25519 is supported
 */
bool Pass::gpgSupportsEd25519(const QString &gpgExecutable) {
  const QString exe =
      gpgExecutable.isEmpty() ? QStringLiteral("gpg") : gpgExecutable;
  QString out, err;
  if (Executor::executeBlocking(exe, {"--version"}, &out, &err) != 0) {
    return false;
  }
  QRegularExpression versionRegex(R"(gpg \(GnuPG\) (\d+)\.(\d+))");
  QRegularExpressionMatch match = versionRegex.match(out);
  if (!match.hasMatch()) {
    return false;
  }
  int major = match.captured(1).toInt();
  int minor = match.captured(2).toInt();
  return major > 2 || (major == 2 && minor >= 1);
}

/**
 * @brief Pass::getDefaultKeyTemplate return default key generation template
 * Uses ed25519 if supported, otherwise falls back to RSA
 * @return GPG batch template string
 */
QString Pass::getDefaultKeyTemplate(const QString &gpgExecutable) {
  if (gpgSupportsEd25519(gpgExecutable)) {
    return QStringLiteral("%echo Generating a default key\n"
                          "Key-Type: EdDSA\n"
                          "Key-Curve: Ed25519\n"
                          "Subkey-Type: ECDH\n"
                          "Subkey-Curve: Curve25519\n"
                          "Name-Real: \n"
                          "Name-Comment: QtPass\n"
                          "Name-Email: \n"
                          "Expire-Date: 0\n"
                          "%no-protection\n"
                          "%commit\n"
                          "%echo done");
  }
  return QStringLiteral("%echo Generating a default key\n"
                        "Key-Type: RSA\n"
                        "Subkey-Type: RSA\n"
                        "Name-Real: \n"
                        "Name-Comment: QtPass\n"
                        "Name-Email: \n"
                        "Expire-Date: 0\n"
                        "%no-protection\n"
                        "%commit\n"
                        "%echo done");
}

namespace {
/**
 * @brief Resolve a candidate gpgconf path from the trailing WSL path segment.
 *
 * Takes the directory portion of @p lastPart (separated by '/' or '\\') and
 * appends "gpgconf"; if no separator is present, returns the bare executable
 * name "gpgconf".
 *
 * @param lastPart Path fragment that may contain a directory and executable.
 * @return Full path ending in "gpgconf", or "gpgconf" as a fallback.
 */
auto resolveWslGpgconfPath(const QString &lastPart) -> QString {
  qsizetype lastSep = lastPart.lastIndexOf('/');
  if (lastSep < 0) {
    lastSep = lastPart.lastIndexOf('\\');
  }
  if (lastSep >= 0) {
    return lastPart.left(lastSep + 1) + "gpgconf";
  }
  return QStringLiteral("gpgconf");
}

/**
 * @brief Finds the path to the gpgconf executable in the same directory as the
 * given GPG path.
 * @example
 * QString result = findGpgconfInGpgDir(gpgPath);
 * std::cout << result.toStdString() << std::endl; // Expected output: path to
 * gpgconf or empty string
 *
 * @param gpgPath - Absolute path to a GPG executable or related file used to
 * locate gpgconf.
 * @return QString - The full path to gpgconf if found and executable; otherwise
 * an empty QString.
 */
QString findGpgconfInGpgDir(const QString &gpgPath) {
  QFileInfo gpgInfo(gpgPath);
  if (!gpgInfo.isAbsolute()) {
    return {};
  }

  QDir dir(gpgInfo.absolutePath());

#ifdef Q_OS_WIN
  QFileInfo candidateExe(dir.filePath("gpgconf.exe"));
  if (candidateExe.isExecutable()) {
    return candidateExe.filePath();
  }
#endif

  QFileInfo candidate(dir.filePath("gpgconf"));
  if (candidate.isExecutable()) {
    return candidate.filePath();
  }
  return {};
}

} // namespace

/**
 * @brief Resolves the appropriate gpgconf command from a given GPG executable
 * path or command string.
 * @example
 * ResolvedGpgconfCommand result = Pass::resolveGpgconfCommand("wsl.exe
 * /usr/bin/gpg"); std::cout << result.first.toStdString() << std::endl; //
 * Expected output sample
 *
 * @param const QString &gpgPath - Path or command string pointing to the GPG
 * executable.
 * @return ResolvedGpgconfCommand - A pair containing the resolved gpgconf
 * command and its arguments.
 */
auto Pass::resolveGpgconfCommand(const QString &gpgPath)
    -> ResolvedGpgconfCommand {
  if (gpgPath.trimmed().isEmpty()) {
    return {"gpgconf", {}};
  }

  // A WSL command: gpgconf next to the gpg the user configured, run in the
  // same distribution and, like everything else, through --exec rather than
  // the distribution's shell. A WSL form that does not parse (`wsl sh -c
  // ...`, a bare `wsl`) falls back to whatever gpgconf is on the Windows
  // PATH, as it always did.
  if (const auto wsl = Executor::parseWslCommand(gpgPath)) {
    if (!QFileInfo(wsl->command).fileName().startsWith("gpg")) {
      return {"gpgconf", {}};
    }
    const auto gpgconf = wsl->with(resolveWslGpgconfPath(wsl->command));
    return {gpgconf.launcher, gpgconf.argv({})};
  }

  const QStringList parts = QProcess::splitCommand(gpgPath);
  if (parts.isEmpty()) {
    return {"gpgconf", {}};
  }
  const QString first = parts.first();
  if (QFileInfo(first).fileName().compare("wsl", Qt::CaseInsensitive) == 0 ||
      QFileInfo(first).fileName().compare("wsl.exe", Qt::CaseInsensitive) ==
          0) {
    return {"gpgconf", {}};
  }

  if (!first.contains('/') && !first.contains('\\')) {
    return {"gpgconf", {}};
  }

  QString gpgconfPath = findGpgconfInGpgDir(first);
  if (!gpgconfPath.isEmpty()) {
    return {gpgconfPath, {}};
  }

  return {"gpgconf", {}};
}

/**
 * @brief Pass::GenerateGPGKeys internal gpg keypair generator . .
 * @param batch GnuPG style configuration string
 */
void Pass::GenerateGPGKeys(QString batch) {
  const QString gpgPath = m_settings.gpgExecutable;
  if (gpgPath.isEmpty()) {
    // No gpg configured: executeWrapper would hand an empty executable to the
    // Executor, which silently drops it (see Executor::execute), leaving the
    // keygen dialog spinning with no feedback. Surface the misconfiguration
    // instead. Deferred via a queued call so we do not re-enter
    // KeygenDialog::done(), which drives key generation synchronously.
    QMetaObject::invokeMethod(
        this,
        [this]() {
          emit generateGPGKeysFailed(tr("No GPG executable configured"));
          emit processErrorExit(1, tr("No GPG executable configured"));
        },
        Qt::QueuedConnection);
    return;
  }

  // Kill any stale GPG agents that might be holding locks on the key database.
  // This helps avoid "database locked" timeouts during key generation.
  ResolvedGpgconfCommand resolvedGpgconf = resolveGpgconfCommand(gpgPath);
  QStringList killArgs = resolvedGpgconf.arguments;
  killArgs << "--kill";
  killArgs << "gpg-agent";
  // Use same environment as key generation to target correct gpg-agent
  if (Executor::executeBlocking(env, resolvedGpgconf.program, killArgs) != 0) {
    qCWarning(lcQtPass) << "Failed to kill gpg-agent";
  }

  executeWrapper(GPG_GENKEYS, gpgPath, {"--gen-key", "--no-tty", "--batch"},
                 std::move(batch), true, true);
}

/**
 * @brief Pass::listKeys list users
 * @param keystrings
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
auto Pass::listKeys(QStringList keystrings, bool secret) -> QList<UserInfo> {
  QStringList args = {"--no-tty", "--with-colons", "--with-fingerprint"};
  args.append(secret ? "--list-secret-keys" : "--list-keys");

  for (const QString &keystring : std::as_const(keystrings)) {
    if (!keystring.isEmpty()) {
      args.append(keystring);
    }
  }
  QString p_out;
  if (Executor::executeBlocking(m_settings.gpgExecutable, args, &p_out) != 0) {
    return {};
  }
  return parseGpgColonOutput(p_out, secret);
}

/**
 * @brief Pass::listKeys list users
 * @param keystring
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
auto Pass::listKeys(const QString &keystring, bool secret) -> QList<UserInfo> {
  return listKeys(QStringList(keystring), secret);
}

/**
 * @brief Maps GPG stderr (which may include --status-fd 2 tokens) to a
 * user-friendly encryption error string.
 *
 * Checked in order: machine-readable [GNUPG:] status tokens first (locale-
 * independent), then case-insensitive substring fallbacks for GPG builds that
 * don't emit status tokens.
 *
 * @param err Raw stderr from GPG
 * @return Translated human-readable error, or empty string if not recognised
 */
namespace {

/**
 * @brief Checks if @p str contains any of the @p patterns (case-sensitive).
 * @param str String to search in.
 * @param patterns Patterns to search for.
 * @return true if any pattern is found, false otherwise.
 */
auto containsAny(const QString &str, const QStringList &patterns) -> bool {
  for (const QString &p : patterns) {
    if (str.contains(p)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Checks if str contains any of the patterns (case-insensitive).
 * @param str String to search in (will be lowercased once).
 * @param patterns List of patterns to search for (must be lowercase; caller
 * should convert patterns to lowercase before calling).
 * @return true if any pattern is found.
 */
auto containsAnyCaseInsensitive(const QString &str, const QStringList &patterns)
    -> bool {
  const QString lower = str.toLower();
  for (const QString &p : patterns) {
    if (lower.contains(p)) {
      return true;
    }
  }
  return false;
}

} // namespace

auto gpgErrorMessage(const QString &err) -> QString {
  // Machine-readable status tokens added by --status-fd 2
  if (containsAny(err, {QStringLiteral("[GNUPG:] KEYEXPIRED"),
                        QStringLiteral("[GNUPG:] INV_RECP 5 ")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: GPG key has expired. Please renew or "
                "replace it.");
  if (containsAny(err, {QStringLiteral("[GNUPG:] KEYREVOKED"),
                        QStringLiteral("[GNUPG:] INV_RECP 4 ")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: GPG key has been revoked.");
  if (containsAny(err, {QStringLiteral("[GNUPG:] NO_PUBKEY"),
                        QStringLiteral("[GNUPG:] INV_RECP")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: recipient GPG key not found or invalid. "
                "Check that the key ID in .gpg-id is correct and imported.");
  if (err.contains(QStringLiteral("[GNUPG:] FAILURE")))
    return QCoreApplication::translate(
        "Pass", "Encryption failed. Check that your GPG key is valid.");

  // Locale-dependent fallbacks
  if (containsAnyCaseInsensitive(err, {QLatin1String("key has expired"),
                                       QLatin1String("key expired")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: GPG key has expired. Please renew or "
                "replace it.");
  if (containsAnyCaseInsensitive(err, {QLatin1String("key has been revoked"),
                                       QLatin1String("revoked")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: GPG key has been revoked.");
  if (containsAnyCaseInsensitive(err, {QLatin1String("no public key"),
                                       QLatin1String("unusable public key"),
                                       QLatin1String("no secret key")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed: recipient GPG key not found or invalid. "
                "Check that the key ID in .gpg-id is correct and imported.");
  if (containsAnyCaseInsensitive(err, {QLatin1String("encryption failed")}))
    return QCoreApplication::translate(
        "Pass", "Encryption failed. Check that your GPG key is valid.");

  return {};
}

namespace {
/**
 * @brief Determine whether a line from `pass grep` output is an entry header.
 *
 * Detects the ANSI blue escape (\x1B[94m) emitted by `pass grep`; as a
 * plain-text fallback, treats a non-indented line ending in ':' as a header.
 *
 * @param rawLine Original unmodified output line (with any ANSI codes).
 * @param trimmedLine The line after surrounding whitespace has been stripped.
 * @return true if the line is an entry header; otherwise false.
 */
auto isGrepHeaderLine(const QString &rawLine, const QString &trimmedLine)
    -> bool {
  return rawLine.startsWith(QStringLiteral("\x1B[94m")) ||
         (!rawLine.startsWith(' ') && !rawLine.startsWith('\t') &&
          trimmedLine.endsWith(':'));
}
} // namespace

/**
 * @brief Parses 'pass grep' raw output into (entry, matches) pairs.
 *
 * pass grep emits ANSI blue color (\x1B[94m) at the start of each entry
 * header line. This is checked before stripping ANSI so headers are detected
 * reliably regardless of locale.
 */
auto parseGrepOutput(const QString &rawOut)
    -> QList<QPair<QString, QStringList>> {
  static const QRegularExpression ansi(
      QStringLiteral(R"(\x1B\[[0-9;]*[a-zA-Z])"));
  QList<QPair<QString, QStringList>> results;
  QString currentEntry;
  QStringList currentMatches;
  for (const QString &rawLine : rawOut.split('\n')) {
    QString line = rawLine;
    line.remove('\r');
    line.remove(ansi);
    line = line.trimmed();
    const bool isHeader = isGrepHeaderLine(rawLine, line);
    if (isHeader) {
      if (!currentEntry.isEmpty() && !currentMatches.isEmpty())
        results.append({currentEntry, currentMatches});
      currentEntry = line.endsWith(':') ? line.chopped(1) : line;
      currentMatches.clear();
    } else if (!currentEntry.isEmpty()) {
      if (!line.isEmpty())
        currentMatches << line;
    }
  }
  if (!currentEntry.isEmpty() && !currentMatches.isEmpty())
    results.append({currentEntry, currentMatches});
  return results;
}

/**
 * @brief Pass::processFinished reemits specific signal based on what process
 * has finished
 * @param id    id of Pass process that was scheduled and finished
 * @param exitCode  return code of a process
 * @param out   output generated by process(if capturing was requested, empty
 *              otherwise)
 * @param err   error output generated by process(if capturing was requested,
 *              or error occurred)
 */
void Pass::finished(int id, int exitCode, const QString &out,
                    const QString &err) {
  auto pid = static_cast<PROCESS>(id);

  // Pop the Show() this completion belongs to whether it worked or not, or
  // the next result would be attributed to the wrong file.
  QString shownFile;
  if (pid == PASS_SHOW && !m_pendingShows.isEmpty()) {
    shownFile = m_pendingShows.dequeue();
  }

  if (exitCode != 0) {
    handleProcessError(pid, exitCode, out, err);
    return;
  }

  if (pid == PASS_SHOW) {
    emit finishedShow(out, shownFile);
    return;
  }
  emitProcessFinishedSignal(pid, out, err);
}

void Pass::handleProcessError(PROCESS pid, int exitCode, const QString &out,
                              const QString &err) {
  Q_UNUSED(out);

  if (pid == PASS_GREP) {
    handleGrepError(exitCode, err);
    return;
  }

  if (pid == PASS_INSERT) {
    const QString friendly = gpgErrorMessage(err);
    if (!friendly.isEmpty()) {
      emit processErrorExit(exitCode, formatInsertError(friendly, err));
      return;
    }
  }

  if (pid == GPG_GENKEYS) {
    emit generateGPGKeysFailed(err);
  }
  emit processErrorExit(exitCode, err);
}

void Pass::handleGrepError(int exitCode, const QString &err) {
  if (exitCode == 1) {
    emit finishedGrep({});
  } else {
    emit processErrorExit(exitCode, err);
    emit finishedGrep({});
  }
}

auto Pass::formatInsertError(const QString &friendly, const QString &err)
    -> QString {
  QStringList humanLines;
  for (const QString &line : err.split('\n')) {
    QString cleanedLine = line;
    cleanedLine.remove('\r');
    if (!cleanedLine.startsWith(QLatin1String("[GNUPG:]")))
      humanLines.append(cleanedLine);
  }
  const QString humanErr = humanLines.join('\n').trimmed();
  return humanErr.isEmpty() ? friendly : friendly + "\n\n" + humanErr;
}

/**
 * @brief Emit the appropriate finished signal for a completed subprocess.
 *
 * Emits a specific Qt signal corresponding to the given process identifier; for
 * grep results the stdout is parsed into a list of matches before emitting.
 *
 * @param pid The process identifier indicating which finished signal to emit.
 * @param out Standard output produced by the process.
 * @param err Standard error produced by the process.
 */
void Pass::emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                     const QString &err) {
  /**
   * @brief Only output that cannot contain a secret reaches the generic
   * listeners (the process output panel among them).
   *
   * An allow list, on purpose: the previous deny list of PASS_SHOW, PASS_GREP
   * and PASS_INSERT meant that any process kind added later was broadcast
   * until someone remembered to exclude it. Now a new kind is silent until
   * someone decides its output is harmless and adds it here.
   */
  switch (pid) {
  case GIT_INIT:
  case Enums::GIT_ADD:
  case Enums::GIT_COMMIT:
  case Enums::GIT_RM:
  case GIT_PULL:
  case GIT_PUSH:
  case Enums::GIT_MOVE:
  case Enums::GIT_COPY:
  case PASS_REMOVE:
  case PASS_INIT:
  case PASS_MOVE:
  case PASS_COPY:
  case GPG_GENKEYS:
    emit finishedAnyWithPid(out, err, pid);
    break;
  case PASS_SHOW:
  case PASS_GREP:
  case PASS_INSERT:
  case Enums::PROCESS_COUNT:
  case Enums::INVALID:
    break;
  }

  switch (pid) {
  case GIT_INIT:
    emit finishedGitInit(out, err);
    break;
  case GIT_PULL:
    emit finishedGitPull(out, err);
    break;
  case GIT_PUSH:
    emit finishedGitPush(out, err);
    break;
  case PASS_SHOW:
    // Handled in finished(), which knows the file.
    break;
  case PASS_INSERT:
    emit finishedInsert(out, err);
    break;
  case PASS_REMOVE:
    emit finishedRemove(out, err);
    break;
  case PASS_INIT:
    emit finishedInit(out, err);
    break;
  case PASS_MOVE:
    emit finishedMove(out, err);
    break;
  case PASS_COPY:
    emit finishedCopy(out, err);
    break;
  case GPG_GENKEYS:
    emit finishedGenerateGPGKeys(out, err);
    break;
  case PASS_GREP:
    emit finishedGrep(parseGrepOutput(out));
    break;
  default:
    qCDebug(lcQtPass) << "Unhandled process type" << pid;
    break;
  }
}

/**
 * @brief Set or remove a single environment variable.
 *
 * @param name Variable name, without a trailing '=' (e.g.
 * "PASSWORD_STORE_DIR").
 * @param value New value; an empty string removes the variable entirely.
 */
void Pass::setEnvVar(const QString &name, const QString &value) {
  if (value.isEmpty())
    env.remove(name);
  else
    env.insert(name, value);
}

/**
 * @brief Update the process environment used for executing external commands.
 *
 * Updates environment entries for PASSWORD_STORE_SIGNING_KEY,
 * PASSWORD_STORE_DIR, PASSWORD_STORE_GENERATED_LENGTH, and
 * PASSWORD_STORE_CHARACTER_SET based on current settings, then applies the
 * environment to the internal executor.
 */
void Pass::updateEnv() {
  setEnvVar(QStringLiteral("PASSWORD_STORE_SIGNING_KEY"),
            m_settings.passSigningKey);
  setEnvVar(QStringLiteral("PASSWORD_STORE_DIR"), m_settings.passStore);

  const PasswordConfiguration &passConfig = m_settings.passwordConfiguration;
  setEnvVar(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH"),
            QString::number(passConfig.length));

  setEnvVar(QStringLiteral("PASSWORD_STORE_CHARACTER_SET"),
            effectiveCharset(passConfig));

  exec.setEnvironment(env);
}

/**
 * @brief Pass::getGpgIdPath return gpgid file path for some file (folder).
 * @param for_file which file (folder) would you like the gpgid file path for.
 * @return path to the gpgid file.
 */
namespace {
/// Whether @p path is @p dir or lies under it, compared the way the
/// platform's file system compares names: "C:/Store" and "c:/store" are the
/// same directory on Windows.
auto isAtOrUnder(const QString &path, const QString &dir,
                 const QString &dirPrefix) -> bool {
#ifdef Q_OS_WIN
  constexpr auto cs = Qt::CaseInsensitive;
#else
  constexpr auto cs = Qt::CaseSensitive;
#endif
  return path.compare(dir, cs) == 0 || path.startsWith(dirPrefix, cs);
}
} // namespace

auto Pass::refuseLinkedPath(const QString &path, bool includeSelf) -> bool {
  const QString full = QDir::isAbsolutePath(path)
                           ? path
                           : QDir(m_settings.passStore).filePath(path);
  if (!Util::isUnderLink(full, m_settings.passStore, includeSelf)) {
    return false;
  }
  const QString why =
      tr("%1 is, or lies behind, a symbolic link or junction. What that "
         "points to is not part of the password store and is left alone.")
          .arg(QDir::toNativeSeparators(QDir::cleanPath(full)));
  // The interface clears the previous entry's panel and text when an
  // operation starts, arms itself (disabled widgets, a pending OTP or copy
  // request, an edit dialog at "Decrypting…") and is released by finished
  // or processErrorExit. A refusal starts nothing, so it says both itself.
  emit startingExecuteWrapper();
  emit critical(tr("Not part of the store"), why);
  emit processErrorExit(1, why);
  return true;
}

auto Pass::getGpgIdPath(const QString &for_file, const QString &passStore)
    -> QString {
  QString normalizedStore = QDir::fromNativeSeparators(passStore);
  QString normalizedFile = QDir::fromNativeSeparators(for_file);
  // Inside the store means at a path boundary: "/store-other/x" is not
  // under "/store", however the prefix compares.
  const QString storeDir = QDir::cleanPath(normalizedStore);
  const QString storePrefix = storeDir.endsWith(QLatin1Char('/'))
                                  ? storeDir
                                  : storeDir + QLatin1Char('/');
  const QString cleanFile = QDir::cleanPath(normalizedFile);
  const bool insideStore = isAtOrUnder(cleanFile, storeDir, storePrefix);
  // A relative name is one inside the store; an absolute path outside it
  // stays what it is and the walk below stops at once.
  const QString fullPath = insideStore || QDir::isAbsolutePath(cleanFile)
                               ? cleanFile
                               : storePrefix + cleanFile;
  QDir gpgIdDir(QFileInfo(fullPath).absoluteDir());
  // QDir::cleanPath() always normalises to forward slashes, so use '/'
  // here rather than QDir::separator() (which returns '\\' on Windows).
  bool found = false;
  while (gpgIdDir.exists()) {
    QString currentPath = QDir::cleanPath(gpgIdDir.absolutePath());
    if (!isAtOrUnder(currentPath, storeDir, storePrefix)) {
      break;
    }
    // A link under the name is not the folder's .gpg-id: whoever planted it
    // chose the recipients elsewhere. The parent's list applies instead.
    const QFileInfo candidate(gpgIdDir.absoluteFilePath(".gpg-id"));
    if (candidate.exists() && !candidate.isSymLink() &&
        !candidate.isJunction()) {
      found = true;
      break;
    }
    if (!gpgIdDir.cdUp()) {
      break;
    }
  }
  return found ? gpgIdDir.absoluteFilePath(".gpg-id")
               : QDir(normalizedStore).filePath(".gpg-id");
}

/**
 * @brief Pass::getRecipientList return list of gpg-id's to encrypt for
 * @param for_file which file (folder) would you like recipients for
 * @return recipients gpg-id contents
 */
auto Pass::recipientsForEditing(const QString &dir, const QString &passStore,
                                QString *warning) -> QStringList {
  if (warning != nullptr) {
    warning->clear();
  }
  const QString gpgIdPath =
      getGpgIdPath(dir.isEmpty() ? QString() : dir, passStore);
  const GpgIdSigner signer(
      m_settings.gpgExecutable,
      GpgIdSigner::keysFromSetting(m_settings.passSigningKey));
  if (!signer.enabled()) {
    return getRecipientList(dir.isEmpty() ? QString() : dir, passStore);
  }
  if (!QFileInfo::exists(gpgIdPath)) {
    return {};
  }
  QByteArray contents;
  if (!signer.verifyFile(gpgIdPath, &contents)) {
    if (warning != nullptr) {
      *warning = tr("The recipient list %1 does not verify against the "
                    "signing key, so nothing is preselected: saving would "
                    "sign whatever is in it. Select the recipients yourself; "
                    "OK writes and signs a fresh list.")
                     .arg(gpgIdPath);
    }
    return {};
  }
  QString why;
  if (!GpgIdGeneration::accept(gpgIdPath, contents, passStore, &why) &&
      warning != nullptr) {
    *warning = why;
  }
  return parseRecipients(contents, gpgIdPath);
}

auto Pass::getRecipientList(const QString &for_file, const QString &passStore)
    -> QStringList {
  const QString gpgIdPath = getGpgIdPath(for_file, passStore);
  // The root .gpg-id comes back whatever it is; a link is not a list.
  if (Util::isLinkedFolder(gpgIdPath)) {
    return {};
  }
  QFile gpgId(gpgIdPath);
  if (!gpgId.open(QIODevice::ReadOnly)) {
    return {};
  }
  return parseRecipients(gpgId.readAll(), gpgId.fileName());
}

auto Pass::parseRecipients(const QByteArray &contents,
                           const QString &sourceName) -> QStringList {
  QStringList recipients;
  const QString text = QString::fromUtf8(contents);
  for (const QString &line : text.split(QLatin1Char('\n'))) {
    QString recipient = line.split(QLatin1Char('#')).first().trimmed();
    if (recipient.isEmpty()) {
      continue;
    }
    if (!Util::isValidKeyId(recipient)) {
      // Never drop a recipient silently: the list is written back verbatim
      // by UsersDialog, so a skipped line disappears from .gpg-id.
      qCWarning(lcQtPass) << "Skipping unusable recipient in" << sourceName
                          << ":" << recipient;
      continue;
    }
    recipients += recipient;
  }
  return recipients;
}

/**
 * @brief Pass::seedGpgIdFile write the inherited recipients into a new
 * folder's .gpg-id
 * @param newDir absolute path of the freshly created folder
 * @param passStore root directory of the password store
 * @return true when newDir/.gpg-id was written
 */
auto Pass::seedGpgIdFile(const QString &newDir, const QString &passStore)
    -> bool {
  const QString gpgIdFile = QDir(newDir).absoluteFilePath(".gpg-id");
  if (QFileInfo::exists(gpgIdFile)) {
    return false;
  }
  // Resolve from the file we are about to create: getGpgIdPath walks up from
  // its directory, so this yields the parent's .gpg-id whether or not newDir
  // carries a trailing separator.
  const QStringList recipients = getRecipientList(gpgIdFile, passStore);
  if (recipients.isEmpty()) {
    return false;
  }
  QSaveFile gpgId(gpgIdFile);
  if (!gpgId.open(QIODevice::WriteOnly)) {
    return false;
  }
  // Only reached without a signing key (MainWindow seeds only then), so no
  // generation header: nothing checks freshness, and a plain list stays
  // readable for clients that take a comment for a recipient.
  QTextStream out(&gpgId);
  for (const QString &recipient : recipients) {
    out << recipient << '\n';
  }
  out.flush();
  if (out.status() != QTextStream::Ok || !gpgId.commit()) {
    return false;
  }
  // Lock to owner-only access; see ImitatePass::writeGpgIdFile for the
  // rationale (NFS / USB / unusual umask). Best-effort where setPermissions
  // is a no-op.
  QFile::setPermissions(gpgIdFile, QFile::ReadOwner | QFile::WriteOwner);
  return true;
}

/* Copyright (C) 2017 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

/**
 * @brief Generates a random number bounded by the given value.
 * @param bound Upper bound (exclusive)
 * @return Random number in range [0, bound)
 */
auto Pass::boundedRandom(quint32 bound) -> quint32 {
  if (bound < 2) {
    return 0;
  }

  quint32 randval;
  // Rejection-sampling threshold to avoid modulo bias.
  // This follows the well-known "arc4random_uniform"-style approach:
  // reject values in the low range [0, min), where
  //   min = 2^32 % bound
  // so that the remaining range size is an exact multiple of `bound`.
  //
  // In quint32 arithmetic, (1 + ~bound) wraps to (2^32 - bound), therefore
  //   (1 + ~bound) % bound == 2^32 % bound.
  const quint32 rejectionThreshold = (1 + ~bound) % bound;

  do {
    randval = QRandomGenerator::system()->generate();
  } while (randval < rejectionThreshold);

  return randval % bound;
}

/**
 * @brief Generates a random password from the given charset.
 * @param charset Characters to use in the password
 * @param length Desired password length
 * @return Generated password string
 */
auto Pass::generateRandomPassword(const QString &charset, unsigned int length)
    -> QString {
  if (charset.isEmpty() || length == 0U) {
    return {};
  }
  QString out;
  for (unsigned int i = 0; i < length; ++i) {
    out.append(charset.at(static_cast<int>(
        boundedRandom(static_cast<quint32>(charset.length())))));
  }
  return out;
}
