// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "pass.h"
#include "gpgkeystate.h"
#include "helpers.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using Enums::GIT_INIT;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::GPG_GENKEYS;
using Enums::PASS_COPY;
using Enums::PASS_GREP;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_OTP_GENERATE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;

/**
 * @brief Pass::Pass wrapper for using either pass or the pass imitation
 */
Pass::Pass() : wrapperRunning(false), env(QProcess::systemEnvironment()) {
  connect(&exec,
          static_cast<void (Executor::*)(int, int, const QString &,
                                         const QString &)>(&Executor::finished),
          this, &Pass::finished);

  // This was previously using direct QProcess signals.
  // The code now uses Executor instead of raw QProcess for better control.
  // connect(&process, SIGNAL(error(QProcess::ProcessError)), this,
  //        SIGNAL(error(QProcess::ProcessError)));

  connect(&exec, &Executor::starting, this, &Pass::startingExecuteWrapper);
  // Merge our vars into WSLENV rather than blindly appending a duplicate entry
  const QStringList wslenvVars = {
      QStringLiteral("PASSWORD_STORE_DIR/p"),
      QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH/w"),
      QStringLiteral("PASSWORD_STORE_CHARACTER_SET/w")};
  const QString wslenvPrefix = QStringLiteral("WSLENV=");
  auto it = std::find_if(env.begin(), env.end(), [&](const QString &s) {
    return s.startsWith(wslenvPrefix);
  });
  if (it == env.end()) {
    env.append(wslenvPrefix + wslenvVars.join(':'));
  } else {
    QStringList parts =
        it->mid(wslenvPrefix.size()).split(':', Qt::SkipEmptyParts);
    for (const QString &v : wslenvVars) {
      if (!parts.contains(v))
        parts.append(v);
    }
    *it = wslenvPrefix + parts.join(':');
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
#ifdef QT_DEBUG
  dbg() << app << args;
#endif
  exec.execute(id, QtPassSettings::getPassStore(), app, args, std::move(input),
               readStdout, readStderr);
}

/**
 * @brief Initializes the pass wrapper environment.
 */
void Pass::init() {
#ifdef __APPLE__
  // If it exists, add the gpgtools to PATH
  if (QFile("/usr/local/MacGPG2/bin").exists())
    env.replaceInStrings("PATH=", "PATH=/usr/local/MacGPG2/bin:");
  // Add missing /usr/local/bin
  if (env.filter("/usr/local/bin").isEmpty())
    env.replaceInStrings("PATH=", "PATH=/usr/local/bin:");
#endif

  if (!QtPassSettings::getGpgHome().isEmpty()) {
    QDir absHome(QtPassSettings::getGpgHome());
    absHome.makeAbsolute();
    env << "GNUPGHOME=" + absHome.path();
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
  QString passwd;
  if (QtPassSettings::isUsePwgen()) {
    // --secure goes first as it overrides --no-* otherwise
    QStringList args;
    args.append("-1");
    if (!QtPassSettings::isLessRandom()) {
      args.append("--secure");
    }
    args.append(QtPassSettings::isAvoidCapitals() ? "--no-capitalize"
                                                  : "--capitalize");
    args.append(QtPassSettings::isAvoidNumbers() ? "--no-numerals"
                                                 : "--numerals");
    if (QtPassSettings::isUseSymbols()) {
      args.append("--symbols");
    }
    args.append(QString::number(length));
    // executeBlocking returns 0 on success, non-zero on failure
    if (Executor::executeBlocking(QtPassSettings::getPwgenExecutable(), args,
                                  &passwd) == 0) {
      static const QRegularExpression literalNewLines{"[\\n\\r]"};
      passwd.remove(literalNewLines);
    } else {
      passwd.clear();
#ifdef QT_DEBUG
      qDebug() << __FILE__ << ":" << __LINE__ << "\t"
               << "pwgen fail";
#endif
      // Error is already handled by clearing passwd; no need for critical
      // signal here
    }
  } else {
    // Validate charset - if CUSTOM is selected but chars are empty,
    // fall back to ALLCHARS to prevent weak passwords (issue #780)
    QString effectiveCharset = charset;
    if (effectiveCharset.isEmpty()) {
      effectiveCharset = QtPassSettings::getPasswordConfiguration()
                             .Characters[PasswordConfiguration::ALLCHARS];
    }
    if (effectiveCharset.length() > 0) {
      passwd = generateRandomPassword(effectiveCharset, length);
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
bool Pass::gpgSupportsEd25519() {
  QString out, err;
  if (Executor::executeBlocking(QtPassSettings::getGpgExecutable(),
                                {"--version"}, &out, &err) != 0) {
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
QString Pass::getDefaultKeyTemplate() {
  if (gpgSupportsEd25519()) {
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
auto resolveWslGpgconfPath(const QString &lastPart) -> QString {
  int lastSep = lastPart.lastIndexOf('/');
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
    return QString();
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
  return QString();
}

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
/**
 * @brief Splits a command string into arguments while respecting quotes and
 * escape characters.
 * @example
 * QStringList result = splitCommandCompat("cmd \"arg one\" 'arg two'
 * escaped\\ space");
 * // Expected output: ["cmd", "arg one", "arg two", "escaped space"]
 *
 * @param command - The input command string to split into individual arguments.
 * @return QStringList - A list of parsed command arguments.
 */
QStringList splitCommandCompat(const QString &command) {
  QStringList result;
  QString current;
  bool inSingleQuote = false;
  bool inDoubleQuote = false;
  bool escaping = false;
  for (QChar ch : command) {
    if (escaping) {
      current.append(ch);
      escaping = false;
      continue;
    }
    if (ch == '\\') {
      escaping = true;
      continue;
    }
    if (ch == '\'' && !inDoubleQuote) {
      inSingleQuote = !inSingleQuote;
      continue;
    }
    if (ch == '"' && !inSingleQuote) {
      inDoubleQuote = !inDoubleQuote;
      continue;
    }
    if (ch.isSpace() && !inSingleQuote && !inDoubleQuote) {
      if (!current.isEmpty()) {
        result.append(current);
        current.clear();
      }
      continue;
    }
    current.append(ch);
  }
  if (escaping) {
    current.append('\\');
  }
  if (!current.isEmpty()) {
    result.append(current);
  }
  return result;
}
#endif

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

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList parts = QProcess::splitCommand(gpgPath);
#else
  QStringList parts = splitCommandCompat(gpgPath);
#endif

  if (parts.isEmpty()) {
    return {"gpgconf", {}};
  }

  const QString first = parts.first();
  if (first == "wsl" || first == "wsl.exe") {
    if (parts.size() >= 2 && parts.at(1).startsWith("sh")) {
      return {"gpgconf", {}};
    }
    if (parts.size() >= 2 &&
        QFileInfo(parts.last()).fileName().startsWith("gpg")) {
      QString wslGpgconf = resolveWslGpgconfPath(parts.last());
      parts.removeLast();
      parts.append(wslGpgconf);
      return {parts.first(), parts.mid(1)};
    }
    return {"gpgconf", {}};
  }

  if (!first.contains('/') && !first.contains('\\')) {
    return {"gpgconf", {}};
  }

  QString gpgconfPath = findGpgconfInGpgDir(gpgPath);
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
  // Kill any stale GPG agents that might be holding locks on the key database
  // This helps avoid "database locked" timeouts during key generation
  QString gpgPath = QtPassSettings::getGpgExecutable();
  if (!gpgPath.isEmpty()) {
    ResolvedGpgconfCommand gpgconf = resolveGpgconfCommand(gpgPath);
    QStringList killArgs = gpgconf.arguments;
    killArgs << "--kill";
    killArgs << "gpg-agent";
    // Use same environment as key generation to target correct gpg-agent
    Executor::executeBlocking(env, gpgconf.program, killArgs);
  }

  executeWrapper(GPG_GENKEYS, gpgPath, {"--gen-key", "--no-tty", "--batch"},
                 std::move(batch));
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

  for (const QString &keystring : AS_CONST(keystrings)) {
    if (!keystring.isEmpty()) {
      args.append(keystring);
    }
  }
  QString p_out;
  if (Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args,
                                &p_out) != 0) {
    return QList<UserInfo>();
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
    // ANSI-colored header starts with the blue escape; plain-text fallback:
    // a non-indented line ending with ':' (pass grep format without color)
    bool isHeader = rawLine.startsWith(QStringLiteral("\x1B[94m")) ||
                    (!rawLine.startsWith(' ') && !rawLine.startsWith('\t') &&
                     line.endsWith(':') && !line.isEmpty());
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

  if (exitCode != 0) {
    handleProcessError(pid, exitCode, out, err);
    return;
  }

  emitProcessFinishedSignal(pid, out, err);
}

void Pass::handleProcessError(PROCESS pid, int exitCode, const QString &out,
                              const QString &err) {
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
  for (QString line : err.split('\n')) {
    line.remove('\r');
    if (!line.startsWith(QLatin1String("[GNUPG:]")))
      humanLines.append(line);
  }
  const QString humanErr = humanLines.join('\n').trimmed();
  return humanErr.isEmpty() ? friendly : friendly + "\n\n" + humanErr;
}

void Pass::emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                     const QString &err) {
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
    emit finishedShow(out);
    break;
  case PASS_OTP_GENERATE:
    emit finishedOtpGenerate(out);
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
#ifdef QT_DEBUG
    dbg() << "Unhandled process type" << pid;
#endif
    break;
  }
}

/**
 * @brief Pass::updateEnv update the execution environment (used when
 * switching profiles)
 */
// key must include the trailing '=' (e.g. "FOO="); env.filter() does substring
// matching so the '=' anchors the lookup to avoid collisions with longer names.
void Pass::setEnvVar(const QString &key, const QString &value) {
  Q_ASSERT(key.endsWith('='));
  const QStringList existing = env.filter(key);
  for (const QString &entry : existing)
    env.removeAll(entry);
  if (!value.isEmpty())
    env.append(key + value);
}

void Pass::updateEnv() {
  setEnvVar(QStringLiteral("PASSWORD_STORE_SIGNING_KEY="),
            QtPassSettings::getPassSigningKey());
  setEnvVar(QStringLiteral("PASSWORD_STORE_DIR="),
            QtPassSettings::getPassStore());

  PasswordConfiguration passConfig = QtPassSettings::getPasswordConfiguration();
  setEnvVar(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH="),
            QString::number(passConfig.length));

  int sel = passConfig.selected;
  if (sel < 0 || sel >= PasswordConfiguration::CHARSETS_COUNT)
    sel = PasswordConfiguration::ALLCHARS;
  QString charset = passConfig.Characters[sel];
  if (charset.isEmpty())
    charset = passConfig.Characters[PasswordConfiguration::ALLCHARS];
  setEnvVar(QStringLiteral("PASSWORD_STORE_CHARACTER_SET="), charset);

  exec.setEnvironment(env);
}

/**
 * @brief Pass::getGpgIdPath return gpgid file path for some file (folder).
 * @param for_file which file (folder) would you like the gpgid file path for.
 * @return path to the gpgid file.
 */
auto Pass::getGpgIdPath(const QString &for_file) -> QString {
  QString passStore =
      QDir::fromNativeSeparators(QtPassSettings::getPassStore());
  QString normalizedFile = QDir::fromNativeSeparators(for_file);
  QString fullPath = normalizedFile.startsWith(passStore)
                         ? normalizedFile
                         : passStore + "/" + normalizedFile;
  QDir gpgIdDir(QFileInfo(fullPath).absoluteDir());
  bool found = false;
  while (gpgIdDir.exists() && gpgIdDir.absolutePath().startsWith(passStore)) {
    if (QFile(gpgIdDir.absoluteFilePath(".gpg-id")).exists()) {
      found = true;
      break;
    }
    if (!gpgIdDir.cdUp()) {
      break;
    }
  }
  QString gpgIdPath(
      found ? gpgIdDir.absoluteFilePath(".gpg-id")
            : QDir(QtPassSettings::getPassStore()).filePath(".gpg-id"));

  return gpgIdPath;
}

/**
 * @brief Pass::getRecipientList return list of gpg-id's to encrypt for
 * @param for_file which file (folder) would you like recipients for
 * @return recipients gpg-id contents
 */
auto Pass::getRecipientList(const QString &for_file) -> QStringList {
  QFile gpgId(getGpgIdPath(for_file));
  if (!gpgId.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  QStringList recipients;
  while (!gpgId.atEnd()) {
    QString recipient(gpgId.readLine());
    recipient = recipient.split("#")[0].trimmed();
    if (!recipient.isEmpty() && Util::isValidKeyId(recipient)) {
      recipients += recipient;
    }
  }
  return recipients;
}

/**
 * @brief Pass::getRecipientString formatted string for use with GPG
 * @param for_file which file (folder) would you like recipients for
 * @param separator formating separator eg: " -r "
 * @param count
 * @return recipient string
 */
auto Pass::getRecipientString(const QString &for_file, const QString &separator,
                              int *count) -> QStringList {
  Q_UNUSED(separator)
  QStringList recipients = Pass::getRecipientList(for_file);
  if (count) {
    *count = recipients.size();
  }
  return recipients;
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
  // Rejection-sampling threshold to avoid modulo bias:
  // In quint32 arithmetic, (1 + ~bound) wraps to (2^32 - bound), so
  // (1 + ~bound) % bound == 2^32 % bound.
  // Values randval < max_mod_bound are rejected; accepted values produce a
  // uniform distribution when reduced with (randval % bound).
  const quint32 max_mod_bound = (1 + ~bound) % bound;

  do {
    randval = QRandomGenerator::system()->generate();
  } while (randval < max_mod_bound);

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