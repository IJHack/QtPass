// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_GPGIDGENERATION_H_
#define SRC_GPGIDGENERATION_H_

#include <QByteArray>
#include <QString>
#include <optional>

/**
 * @brief Rollback detection for signed `.gpg-id` files.
 *
 * A signature proves that a recipient list is authentic and unmodified; it
 * does not prove that it is the current one. Whoever can write to a shared
 * store can put back an older, genuinely signed list that still names a
 * member since removed, and every later encryption would include them again.
 *
 * So a `.gpg-id` QtPass writes starts with one comment line,
 * `# QtPass-GpgId-Generation: N`, that `pass` and QtPass's own recipient
 * parser ignore and the signature covers. QtPass remembers, per `.gpg-id`
 * file, the highest generation it has accepted, and refuses a signed list
 * whose generation is lower.
 *
 * What this is and is not: rollback detection on a device that has seen the
 * newer list. A device seeing a store for the first time, or one that was
 * offline, has nothing to compare against. The generation is monotonic per
 * observer, not a distributed sequence number: two devices can both produce
 * generation 19 from 18, and Git is what sorts that out. A legitimate
 * revert to an older signed list is refused too, on purpose; saving the
 * recipients again writes a higher generation and is the way through.
 */
class GpgIdGeneration {
public:
  /// The one line format. The grammar is strict: this prefix, one ASCII
  /// decimal number of at most 18 digits, nothing else on the line.
  static const QByteArray kPrefix;

  /**
   * @brief The generation a `.gpg-id`'s bytes declare.
   * @param contents The file, as verified.
   * @param error Receives why the metadata is unusable, if not null.
   * @return 0 when no generation line is present (a list written before
   *         generations existed, or by `pass`); the number when exactly one
   *         well-formed line is; nothing when a line is malformed or there
   *         is more than one, which is not a list to trust.
   */
  static auto parse(const QByteArray &contents, QString *error = nullptr)
      -> std::optional<qint64>;

  /**
   * @brief The generation line for @p generation, newline included.
   */
  static auto header(qint64 generation) -> QByteArray;

  /**
   * @brief @p recipients (one per line, no generation line) with the
   * generation line in front.
   */
  static auto withHeader(qint64 generation, const QByteArray &recipients)
      -> QByteArray;

  /**
   * @brief The highest generation this device has accepted or written for
   * the `.gpg-id` at @p gpgIdFile; 0 when it has never seen one.
   */
  static auto remembered(const QString &gpgIdFile) -> qint64;

  /**
   * @brief Record @p generation as accepted for @p gpgIdFile, written
   * through to disk before returning: a generation only in memory when the
   * process dies would let the old list back in on the next start.
   * @return false when the record could not be written; the caller must
   *         then treat the list as not accepted.
   */
  static auto remember(const QString &gpgIdFile, qint64 generation) -> bool;

  /**
   * @brief Decide about a verified list: its generation against the
   * remembered one, and remember it when it passes.
   * @param gpgIdFile The `.gpg-id` the bytes came from.
   * @param contents The verified bytes.
   * @param error Receives the reason for a refusal, if not null.
   * @return true when the list may be used.
   */
  static auto accept(const QString &gpgIdFile, const QByteArray &contents,
                     QString *error = nullptr) -> bool;

  /**
   * @brief The generation to write next for @p gpgIdFile: one above both
   * what the file on disk declares (0 when it declares nothing usable) and
   * what this device remembers.
   */
  static auto next(const QString &gpgIdFile) -> qint64;

  /**
   * @brief The settings key for @p gpgIdFile: a hash of its canonical path,
   * so path spelling (case, separators, links on the way) and QSettings'
   * treatment of `/` do not matter.
   */
  static auto key(const QString &gpgIdFile) -> QString;
};

#endif // SRC_GPGIDGENERATION_H_
