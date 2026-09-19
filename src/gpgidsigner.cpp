// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidsigner.h"

#include "executor.h"
#include <QFile>
#include <QRegularExpression>
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

auto GpgIdSigner::sign(const QString &gpgIdFile, QString *error) const -> bool {
  if (!enabled()) {
    return true;
  }
  if (m_keys.size() > 1) {
    qCDebug(lcQtPass)
        << "Multiple signing keys configured; using only the first key:"
        << m_keys.first();
  }
  const int rc = run({QStringLiteral("--default-key"), m_keys.first(),
                      QStringLiteral("--yes"), QStringLiteral("--detach-sign"),
                      QStringLiteral("--"),
                      Executor::translatePathForWsl(gpgIdFile, m_gpg)},
                     nullptr, error);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG signing failed with code:" << rc;
    return false;
  }
  return true;
}

auto GpgIdSigner::validSigFingerprints(const QString &statusOutput)
    -> QStringList {
  static const QRegularExpression re(
      // 40 hex for v4 keys, 64 for v5/v6 (gpg 2.5+), either position.
      R"(^\[GNUPG:\] VALIDSIG ([A-F0-9]{40}(?:[A-F0-9]{24})?) .* )"
      R"(([A-F0-9]{40}(?:[A-F0-9]{24})?)\r?$)",
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch m = re.match(statusOutput);
  if (!m.hasMatch()) {
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
  QFile file(gpgIdFile);
  if (!file.open(QIODevice::ReadOnly)) {
    contents->clear();
    return false;
  }
  *contents = file.readAll();
  return verify(*contents, gpgIdFile + QStringLiteral(".sig"));
}
