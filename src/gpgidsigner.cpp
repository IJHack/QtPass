// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgidsigner.h"
#include "executor.h"
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
  return passSigningKey.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

auto GpgIdSigner::run(const QStringList &args, QString *out, QString *err) const
    -> int {
  return m_exec(m_gpg, args, QString(), out, err);
}

auto GpgIdSigner::haveSecretKey() const -> bool {
  QString out;
  const QStringList args = QStringList{QStringLiteral("--status-fd=1"),
                                       QStringLiteral("--list-secret-keys")} +
                           m_keys;
  const int rc = run(args, &out);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG list-secret-keys failed with code:" << rc;
    return false;
  }
  for (const QString &key : m_keys) {
    if (out.contains(QStringLiteral("[GNUPG:] KEY_CONSIDERED ") + key)) {
      return true;
    }
  }
  return false;
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
      R"(^\[GNUPG:\] VALIDSIG ([A-F0-9]{40}) .* ([A-F0-9]{40})\r?$)",
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch m = re.match(statusOutput);
  if (!m.hasMatch()) {
    return {};
  }
  return {m.captured(1), m.captured(2)};
}

auto GpgIdSigner::verify(const QString &gpgIdFile) const -> bool {
  if (!enabled()) {
    return true;
  }
  QString out;
  const QString file = Executor::translatePathForWsl(gpgIdFile, m_gpg);
  const int rc =
      run({QStringLiteral("--verify"), QStringLiteral("--status-fd=1"),
           file + QStringLiteral(".sig"), file},
          &out);
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
