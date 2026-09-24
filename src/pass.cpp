// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "pass.h"
#include "gpgidgeneration.h"
#include "gpgidsigner.h"
#include "gpgkeystate.h"
#include "processinfo.h"
#include "util.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
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
auto fallbackCharset(const QString &input, const QString &fallback) -> QString {
  return input.isEmpty() ? fallback : input;
}

auto effectiveCharset(const PasswordConfiguration &passConfig) -> QString {
  int sel = passConfig.selected;
  if (sel < 0 || sel >= PasswordConfiguration::CHARSETS_COUNT)
    sel = PasswordConfiguration::ALLCHARS;
  return fallbackCharset(
      passConfig.Characters[sel],
      passConfig.Characters[PasswordConfiguration::ALLCHARS]);
}
} // namespace

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

  // The configured gpgHome wins over GNUPGHOME only when it exists: a stale
  // one (#1711) fails every gpg call with "No secret key". Clearing the
  // setting restores the inherited value rather than keeping the old path.
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
    if (Executor::executeBlocking(m_settings.pwgenExecutable, args, &passwd) ==
        0) {
      static const QRegularExpression literalNewLines{"[\\n\\r]"};
      passwd.remove(literalNewLines);
    } else {
      passwd.clear();
      qCDebug(lcQtPass) << "pwgen fail";
    }
  } else {
    // An empty CUSTOM charset falls back to ALLCHARS, not a weak password
    // (#780).
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

// ed25519 (GnuPG 2.1+) generates much faster than RSA.
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

auto Pass::resolveGpgconfCommand(const QString &gpgPath)
    -> ResolvedGpgconfCommand {
  if (gpgPath.trimmed().isEmpty()) {
    return {"gpgconf", {}};
  }

  // WSL: gpgconf next to the configured gpg, same distribution, via --exec
  // not the shell. An unparsed form (`wsl sh -c ...`, bare `wsl`) falls back
  // to gpgconf on the Windows PATH.
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

void Pass::GenerateGPGKeys(QString batch) {
  const QString gpgPath = m_settings.gpgExecutable;
  if (gpgPath.isEmpty()) {
    // The Executor silently drops an empty executable, leaving the keygen
    // dialog spinning. Queued so we do not re-enter KeygenDialog::done(),
    // which drives key generation synchronously.
    QMetaObject::invokeMethod(
        this,
        [this]() {
          emit generateGPGKeysFailed(tr("No GPG executable configured"));
          emit processErrorExit(1, tr("No GPG executable configured"));
        },
        Qt::QueuedConnection);
    return;
  }

  // A stale gpg-agent holding the key database lock causes "database locked"
  // timeouts during key generation.
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

auto Pass::listKeys(const QString &keystring, bool secret) -> QList<UserInfo> {
  return listKeys(QStringList(keystring), secret);
}

namespace {

auto containsAny(const QString &str, const QStringList &patterns) -> bool {
  for (const QString &p : patterns) {
    if (str.contains(p)) {
      return true;
    }
  }
  return false;
}

// @p patterns must already be lowercase; only @p str is lowered.
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
// `pass grep` starts entry headers with ANSI blue (\x1B[94m); without colour,
// a non-indented line ending in ':' is taken as one.
auto isGrepHeaderLine(const QString &rawLine, const QString &trimmedLine)
    -> bool {
  return rawLine.startsWith(QStringLiteral("\x1B[94m")) ||
         (!rawLine.startsWith(' ') && !rawLine.startsWith('\t') &&
          trimmedLine.endsWith(':'));
}
} // namespace

// Headers are detected on the raw line, before ANSI is stripped, so the
// colour marks them regardless of locale.
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

void Pass::emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                     const QString &err) {
  // Only output that cannot contain a secret reaches the generic listeners
  // (the output panel among them); Enums::processInfo is the one table.
  if (pid >= 0 && pid < Enums::PROCESS_COUNT &&
      !Enums::processInfo(pid).secret) {
    emit finishedAnyWithPid(out, err, pid);
  }

  // The kind-specific signal. PASS_SHOW is emitted by finished(), which
  // knows the file; PASS_GREP carries parsed results, not raw output.
  using Signal = void (Pass::*)(const QString &, const QString &);
  static const QHash<int, Signal> kSignals{
      {GIT_INIT, &Pass::finishedGitInit},
      {GIT_PULL, &Pass::finishedGitPull},
      {GIT_PUSH, &Pass::finishedGitPush},
      {PASS_INSERT, &Pass::finishedInsert},
      {PASS_REMOVE, &Pass::finishedRemove},
      {PASS_INIT, &Pass::finishedInit},
      {PASS_MOVE, &Pass::finishedMove},
      {PASS_COPY, &Pass::finishedCopy},
      {GPG_GENKEYS, &Pass::finishedGenerateGPGKeys},
  };
  if (pid == PASS_GREP) {
    emit finishedGrep(parseGrepOutput(out));
    return;
  }
  if (const Signal signal = kSignals.value(pid, nullptr)) {
    (this->*signal)(out, err);
  }
}

void Pass::setEnvVar(const QString &name, const QString &value) {
  if (value.isEmpty())
    env.remove(name);
  else
    env.insert(name, value);
}

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
  // The interface arms itself when an operation starts and is released by
  // finished or processErrorExit; a refusal starts nothing, so emits both.
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
  // A folder (trailing separator, which cleanPath() dropped, or an existing
  // directory) wants its own list; an entry wants its folder's.
  const bool isFolder =
      normalizedFile.endsWith(QLatin1Char('/')) || QFileInfo(fullPath).isDir();
  QDir gpgIdDir(isFolder ? fullPath : QFileInfo(fullPath).absolutePath());
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

auto Pass::recipientsForEditing(const QString &dir, const QString &passStore)
    -> RecipientsForEditing {
  RecipientsForEditing result;
  const QString folder = dir.isEmpty() ? QString() : dir;
  const QString gpgIdPath = getGpgIdPath(folder, passStore);
  const GpgIdSigner signer(
      m_settings.gpgExecutable,
      GpgIdSigner::keysFromSetting(m_settings.passSigningKey));
  if (!signer.enabled()) {
    result.state = RecipientsForEditing::State::Unsigned;
    result.recipients = getRecipientList(folder, passStore);
    return result;
  }
  if (!QFileInfo::exists(gpgIdPath)) {
    // A folder without a list yet (new store, new profile): nothing to
    // verify and nothing to preselect.
    result.state = RecipientsForEditing::State::Verified;
    return result;
  }
  QByteArray contents;
  if (!signer.verifyFile(gpgIdPath, &contents)) {
    result.state = RecipientsForEditing::State::Rejected;
    result.warning =
        tr("The recipient list %1 does not verify against the signing key, "
           "so nothing is preselected: saving would sign whatever is in it. "
           "Select the recipients yourself; OK writes and signs a fresh list.")
            .arg(gpgIdPath);
    return result;
  }
  QString why;
  switch (GpgIdGeneration::accept(gpgIdPath, contents, passStore, &why)) {
  case GpgIdGeneration::Verdict::Accepted:
    result.state = RecipientsForEditing::State::Verified;
    result.recipients = parseRecipients(contents, gpgIdPath);
    break;
  case GpgIdGeneration::Verdict::Rollback:
    // Authentic and this folder's, only older: the recovery goes through
    // this dialog, so it is preselected, with the reason in view.
    result.state = RecipientsForEditing::State::VerifiedRollback;
    result.recipients = parseRecipients(contents, gpgIdPath);
    result.warning = why;
    break;
  case GpgIdGeneration::Verdict::Unbound:
  case GpgIdGeneration::Verdict::Conflict:
  case GpgIdGeneration::Verdict::WrongFolder:
  case GpgIdGeneration::Verdict::Malformed:
  case GpgIdGeneration::Verdict::RecordUnavailable:
    // Signed, but not shown to be this folder's list, not a list to trust,
    // or nothing to judge it by: preselecting it would sign it in.
    result.state = RecipientsForEditing::State::Rejected;
    result.warning =
        tr("%1 Nothing is preselected: saving would sign whatever is in it. "
           "Select the recipients yourself.")
            .arg(why);
    break;
  }
  return result;
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

auto Pass::seedGpgIdFile(const QString &newDir, const QString &passStore)
    -> bool {
  const QString gpgIdFile = QDir(newDir).absoluteFilePath(".gpg-id");
  if (QFileInfo::exists(gpgIdFile)) {
    return false;
  }
  // Resolving from the file to be created finds the parent's .gpg-id whether
  // or not newDir has a trailing separator.
  const QStringList recipients = getRecipientList(gpgIdFile, passStore);
  if (recipients.isEmpty()) {
    return false;
  }
  // Only reached without a signing key, so no generation header: a plain
  // list stays readable for clients that take a comment for a recipient.
  // Staged and replacing nothing: a link planted after the exists() above
  // (a dangling one passes it) fails the seed instead of being written through.
  return Util::writeFileReplacing(
      gpgIdFile,
      (recipients.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), false);
}

/* Copyright (C) 2017 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

auto Pass::boundedRandom(quint32 bound) -> quint32 {
  if (bound < 2) {
    return 0;
  }

  quint32 randval;
  // arc4random_uniform-style rejection against modulo bias: drop values
  // below 2^32 % bound, which (1 + ~bound) % bound computes in quint32.
  const quint32 rejectionThreshold = (1 + ~bound) % bound;

  do {
    randval = QRandomGenerator::system()->generate();
  } while (randval < rejectionThreshold);

  return randval % bound;
}

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
