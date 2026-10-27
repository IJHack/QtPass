// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidsigner.h"

#include "executor.h"
#include "util.h"
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <utility>

#include "qtpasslogging.h"

GpgIdSigner::GpgIdSigner(QString gpgExecutable, QStringList signingKeys,
                         Exec exec)
    : m_gpg(std::move(gpgExecutable)), m_keys(std::move(signingKeys)),
      m_exec(std::move(exec)) {
  if (!m_exec) {
    m_exec = [](const QString &app, const QStringList &args,
                const QString &input, QString *out, QString *err) {
      return Executor::executeBlocking(app, args, input, out, err);
    };
  }
}

auto GpgIdSigner::keysFromSetting(const QString &passSigningKey)
    -> QStringList {
  QStringList keys = passSigningKey.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  // gpg prints fingerprints in upper case in its status lines; accept them
  // typed either way.
  for (QString &key : keys) {
    key = key.toUpper();
  }
  return keys;
}

auto GpgIdSigner::run(const QStringList &args, QString *out, QString *err,
                      const QString &input) const -> int {
  return m_exec(m_gpg, args, input, out, err);
}

auto GpgIdSigner::haveSecretKey() const -> bool {
  if (!enabled()) {
    return false;
  }
  // Only the first key signs (see sign()), so only its secret key matters:
  // a usable second key would let Init start and fail at the signature.
  const QString &key = m_keys.first();
  QString out;
  const int rc = run({QStringLiteral("--status-fd=1"),
                      QStringLiteral("--list-secret-keys"), key},
                     &out);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG list-secret-keys failed with code:" << rc;
    return false;
  }
  return out.contains(QStringLiteral("[GNUPG:] KEY_CONSIDERED ") + key);
}

auto GpgIdSigner::sign(const QString &gpgIdFile, const QByteArray &contents,
                       QString *error) const -> bool {
  if (!enabled()) {
    return true;
  }
  if (m_keys.size() > 1) {
    qCDebug(lcQtPass)
        << "Multiple signing keys configured; using only the first key:"
        << m_keys.first();
  }
  // The executor feeds stdin as UTF-8 text (see verify()).
  const QString text = QString::fromUtf8(contents);
  if (text.toUtf8() != contents) {
    if (error)
      *error = QCoreApplication::translate(
          "GpgIdSigner", "the recipient list is not valid UTF-8");
    return false;
  }
  QTemporaryDir scratch;
  if (!scratch.isValid()) {
    if (error)
      *error = scratch.errorString();
    return false;
  }
  const QString output = scratch.filePath(QStringLiteral("sig"));
  // --local-user, not --default-key: without that secret key gpg would
  // print "all values passed to '--default-key' ignored", sign with
  // whatever secret key it has and exit 0.
  const int rc = run(
      {QStringLiteral("--local-user"), m_keys.first(), QStringLiteral("--yes"),
       QStringLiteral("--detach-sign"), QStringLiteral("--output"),
       Executor::translatePathForWsl(output, m_gpg), QStringLiteral("-")},
      nullptr, error, text);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG signing failed with code:" << rc;
    return false;
  }
  QFile signature(output);
  if (!signature.open(QIODevice::ReadOnly) || signature.size() <= 0) {
    if (error)
      *error =
          QCoreApplication::translate("GpgIdSigner", "gpg wrote no signature");
    return false;
  }
  const QByteArray bytes = signature.readAll();
  signature.close();
  // Checked before it is placed: a signature by any key but the configured
  // ones (an older gpg, a wrapper, a smartcard picking its own) is not
  // written, and the one there, if any, stays.
  if (!verify(contents, output)) {
    if (error)
      *error = QCoreApplication::translate(
          "GpgIdSigner",
          "the new signature does not verify as one of the signing keys");
    return false;
  }
  return Util::writeFileReplacing(gpgIdFile + QStringLiteral(".sig"), bytes,
                                  true, error);
}

auto GpgIdSigner::validSigFingerprints(const QString &statusOutput)
    -> QStringList {
  static const QRegularExpression re(
      // 40 hex for v4 keys, 64 for v5/v6 (gpg 2.5+), either position.
      R"(^\[GNUPG:\] VALIDSIG ([A-F0-9]{40}(?:[A-F0-9]{24})?) .* )"
      R"(([A-F0-9]{40}(?:[A-F0-9]{24})?)\r?$)",
      QRegularExpression::MultilineOption);
  // gpg reports a signature by a revoked key as REVKEYSIG plus KEYREVOKED,
  // and still prints VALIDSIG and exits 0. A signing key revoked because it
  // was compromised must not vouch for a recipient list, so a signature
  // whose block (from one NEWSIG to the next) says so does not count. One
  // by an expired key does, as gpg calls it a good signature: expiry is not
  // compromise, and pass accepts it too.
  static const QRegularExpression revoked(
      QStringLiteral(R"(^\[GNUPG:\] (?:REVKEYSIG|KEYREVOKED)\b)"),
      QRegularExpression::MultilineOption);
  static const QRegularExpression newSig(
      QStringLiteral(R"(^\[GNUPG:\] NEWSIG\b)"),
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch m = re.match(statusOutput);
  if (!m.hasMatch()) {
    return {};
  }
  const qsizetype from =
      statusOutput.lastIndexOf(newSig, m.capturedStart()) < 0
          ? 0
          : statusOutput.lastIndexOf(newSig, m.capturedStart());
  const qsizetype to = statusOutput.indexOf(newSig, m.capturedEnd());
  const QString block = statusOutput.mid(from, to < 0 ? -1 : to - from);
  if (revoked.match(block).hasMatch()) {
    return {};
  }
  return {m.captured(1), m.captured(2)};
}

auto GpgIdSigner::verify(const QByteArray &contents,
                         const QString &signatureFile) const -> bool {
  if (!enabled()) {
    return true;
  }
  // The executor feeds stdin as UTF-8 text. A .gpg-id is one: key IDs,
  // fingerprints, e-mail addresses, comments. Bytes that would not survive
  // the round trip cannot be verified faithfully, so they are not verified
  // at all rather than as something else.
  const QString text = QString::fromUtf8(contents);
  if (text.toUtf8() != contents) {
    qCDebug(lcQtPass) << "Refusing to verify" << signatureFile
                      << ": the signed file is not valid UTF-8";
    return false;
  }
  // Same for the signature: a valid pair replayed from elsewhere through two
  // links is not this folder's list.
  if (Util::isLinkedFolder(signatureFile)) {
    qCDebug(lcQtPass) << "Refusing to verify" << signatureFile
                      << ": the signature file is a link";
    return false;
  }
  // A regular file of bounded size, as the list itself is read: a FIFO
  // planted under the name would block gpg, and the GUI thread waiting for
  // it, and a sparse file of gigabytes would be read whole.
  {
    QFile probe;
    if (!Util::openRegularFile(signatureFile, probe) ||
        probe.size() > kMaxSignatureBytes) {
      qCDebug(lcQtPass) << "Refusing to verify" << signatureFile
                        << ": not a regular file of at most"
                        << kMaxSignatureBytes << "bytes";
      return false;
    }
  }
  QString out;
  const QString sig = Executor::translatePathForWsl(signatureFile, m_gpg);
  // "-" makes gpg read the signed data from stdin: the bytes we hold.
  const int rc =
      run({QStringLiteral("--verify"), QStringLiteral("--status-fd=1"),
           QStringLiteral("--"), sig, QStringLiteral("-")},
          &out, nullptr, text);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG verify failed with code:" << rc;
    return false;
  }
  const QStringList fingerprints = validSigFingerprints(out);
  for (const QString &key : m_keys) {
    if (fingerprints.contains(key)) {
      return true;
    }
  }
  return false;
}

auto GpgIdSigner::verifyFile(const QString &gpgIdFile,
                             QByteArray *contents) const -> bool {
  // A link under the name is not the store's .gpg-id, whatever it points
  // to and however well that is signed.
  if (Util::isLinkedFolder(gpgIdFile)) {
    contents->clear();
    return false;
  }
  // Read as the regular file it is and bounded: a FIFO would block, a
  // sparse file of gigabytes be read whole, before any signature is checked.
  std::optional<QByteArray> bytes =
      Util::readRegularFile(gpgIdFile, Util::kMaxGpgIdBytes);
  if (!bytes) {
    contents->clear();
    return false;
  }
  *contents = *std::move(bytes);
  return verify(*contents, gpgIdFile + QStringLiteral(".sig"));
}
