// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements GpgIdSigner on top of gpg's `--status-fd` output.
 */
#include "gpgidsigner.h"

#include "executor.h"
#include "util.h"
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <algorithm>
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
  const int rc =
      run({QStringLiteral("--with-colons"), QStringLiteral("--fixed-list-mode"),
           QStringLiteral("--list-secret-keys"), QStringLiteral("--"), key},
          &out);
  if (rc != 0) {
    qCDebug(lcQtPass) << "GPG list-secret-keys failed with code:" << rc;
    return false;
  }
  // Records, one per line: a key (sec, or ssb for a subkey) and later its
  // own fpr (an rvk may come between). A user ID is a record of its own,
  // with its colons and line breaks escaped, so nothing a key's owner
  // wrote can pass for either; a search of the whole output for the
  // fingerprint could be fooled by one. A key signs when it can (s in its
  // own capabilities), is not revoked, expired or invalid, and its secret
  // part is here (field 15 is # for a stub).
  // Configured as a subkey, that subkey must sign. Configured as a primary
  // key, gpg's --local-user picks a signing key from its whole block, so
  // the primary or any of its subkeys will do.
  struct Key {
    bool signs = false;
    bool valid = false;
    bool primary = false;
    QString fpr;
  };
  QList<QList<Key>> blocks;
  bool awaitingFpr = false;
  const QStringList lines = out.split(QLatin1Char('\n'));
  for (const QString &line : lines) {
    const QStringList fields = line.trimmed().split(QLatin1Char(':'));
    const QString &type = fields.first();
    const bool primary = type == QLatin1String("sec");
    if (primary || type == QLatin1String("ssb")) {
      if (primary) {
        blocks.append(QList<Key>());
      }
      if (blocks.isEmpty()) {
        awaitingFpr = false;
        continue;
      }
      const QString validity = fields.value(1);
      Key k;
      k.primary = primary;
      k.valid = validity != QLatin1String("r") &&
                validity != QLatin1String("e") &&
                validity != QLatin1String("i");
      k.signs = k.valid && fields.value(11).contains(QLatin1Char('s')) &&
                fields.value(14) != QLatin1String("#");
      blocks.last().append(k);
      awaitingFpr = true;
    } else if (type == QLatin1String("fpr") && awaitingFpr) {
      blocks.last().last().fpr = fields.value(9);
      awaitingFpr = false;
    }
  }
  for (const QList<Key> &block : std::as_const(blocks)) {
    // A revoked or expired primary key takes its subkeys with it.
    if (block.isEmpty() || !block.first().valid) {
      continue;
    }
    for (const Key &k : block) {
      if (k.fpr.compare(key, Qt::CaseInsensitive) != 0) {
        continue;
      }
      if (k.signs) {
        return true;
      }
      if (k.primary && std::any_of(block.cbegin(), block.cend(),
                                   [](const Key &sub) { return sub.signs; })) {
        return true;
      }
    }
  }
  return false;
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
  // The signature is in this call's own scratch folder, outside the store:
  // checked by a copy of this signer without the store root.
  GpgIdSigner scratchSigner = *this;
  scratchSigner.m_storeRoot.clear();
  if (!scratchSigner.verify(contents, output)) {
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

auto GpgIdSigner::readOwnFile(const QString &path, qint64 maxBytes) const
    -> std::optional<QByteArray> {
  return m_storeRoot.isEmpty()
             ? Util::readRegularFile(path, maxBytes)
             : Util::readRegularFileInStore(m_storeRoot, path, maxBytes);
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
  // Read as a regular file of bounded size, as the list itself is, and
  // give gpg a copy of those bytes: a FIFO planted under the name would
  // block gpg, and the GUI thread waiting for it, a sparse file of
  // gigabytes would be read whole, and either could be swapped in between
  // a check of the name and gpg opening it.
  const std::optional<QByteArray> signature =
      readOwnFile(signatureFile, kMaxSignatureBytes);
  if (!signature) {
    qCDebug(lcQtPass) << "Refusing to verify" << signatureFile
                      << ": not a regular file of at most" << kMaxSignatureBytes
                      << "bytes";
    return false;
  }
  QTemporaryDir scratch;
  if (!scratch.isValid()) {
    return false;
  }
  const QString copy = scratch.filePath(QStringLiteral("sig"));
  {
    QFile file(copy);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(*signature) != signature->size()) {
      return false;
    }
  }
  QString out;
  const QString sig = Executor::translatePathForWsl(copy, m_gpg);
  // "-" makes gpg read the signed data from stdin: the bytes we hold.
  // --batch stops at the first bad signature; without it gpg checked every
  // packet in the file, each against the key it names, however slow.
  const int rc = run({QStringLiteral("--batch"), QStringLiteral("--verify"),
                      QStringLiteral("--status-fd=1"), QStringLiteral("--"),
                      sig, QStringLiteral("-")},
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
      readOwnFile(gpgIdFile, Util::kMaxGpgIdBytes);
  if (!bytes) {
    contents->clear();
    return false;
  }
  *contents = *std::move(bytes);
  return verify(*contents, gpgIdFile + QStringLiteral(".sig"));
}
