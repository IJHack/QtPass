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
 * does not prove that it is the current one, nor that it was written for
 * the folder it sits in. Whoever can write to a shared store can put back an
 * older, genuinely signed list that still names a member since removed, or
 * copy such a pair into a folder that never had a list of its own, and every
 * later encryption there would include them again.
 *
 * So a `.gpg-id` QtPass writes for a store with a signing key starts with two
 * comment lines, `# QtPass-GpgId-Generation: N` and
 * `# QtPass-GpgId-Folder: <store-relative folder>`, which `pass` (1.7.4 and
 * later), QtPass (1.8 and later) and QtPass's own recipient parser ignore
 * and the signature covers. QtPass remembers, per `.gpg-id` file, the
 * highest generation it has accepted or written, and refuses a signed list
 * whose generation is lower or whose folder is not the one it sits in.
 * Stores without a signing key get plain lists, as before: nothing checks
 * their freshness, and older clients (pass up to 1.7.3, Android Password
 * Store) take a comment for a recipient.
 *
 * What this is and is not: rollback detection on a device that has seen the
 * newer list. A device seeing a store for the first time, or one that was
 * offline, has nothing to compare against. The generation is monotonic per
 * observer, not a distributed sequence number: two devices can both produce
 * generation 19 from 18, and Git is what sorts that out; a device that
 * accepted one of the two then meets the other as the same generation with
 * different bytes, which is a conflict, not a rollback, and is refused until
 * a holder of the signing key saves the recipients once more. For that the
 * record keeps, next to the generation, a digest of the exact bytes it
 * accepted or wrote. A list without the lines (written by `pass`, or by
 * QtPass before 2.0) is generation 0 and is refused once anything higher
 * was accepted; a holder of the signing key saving the recipients from
 * QtPass writes the next generation. Without a readable and writable record
 * nothing is accepted or written: unknown state is not "never seen".
 */
class GpgIdGeneration {
public:
  /// The generation line. The grammar is strict: this prefix, one ASCII
  /// decimal number of at most 18 digits, nothing else on the line.
  static const QByteArray kGenerationPrefix;
  /// The folder line: this prefix and the store-relative folder the list was
  /// written for, `.` for the store root, `/`-separated.
  static const QByteArray kFolderPrefix;
  /// The largest generation the grammar admits (18 nines). reserveNext()
  /// never goes above it: a header QtPass writes must be one it reads back.
  static const qint64 kMaxGeneration;

  /// What a list's header says.
  struct Header {
    /// 0 when the list carries no generation line.
    qint64 generation = 0;
    /// Nothing when the list carries no folder line.
    std::optional<QString> folder;
  };

  /**
   * @brief The header a `.gpg-id`'s bytes declare.
   * @param contents The file, as verified.
   * @param error Receives why the metadata is unusable, if not null.
   * @return The header (generation 0 and no folder for a list written
   *         before generations existed, or by `pass`); nothing when a line
   *         is malformed, appears twice, or a generation line comes without
   *         a folder line, which is not a list to trust.
   */
  static auto parse(const QByteArray &contents, QString *error = nullptr)
      -> std::optional<Header>;

  /**
   * @brief The two header lines for a list.
   * @param generation The generation to declare.
   * @param folder The store-relative folder the list is written for.
   * @return Both lines, newlines included.
   */
  static auto header(qint64 generation, const QString &folder) -> QByteArray;

  /**
   * @brief A complete list: the header in front of the recipients.
   * @param generation The generation to declare.
   * @param folder The store-relative folder the list is written for.
   * @param recipients The recipient lines, one per line, no header.
   * @return The bytes to write.
   */
  static auto withHeader(qint64 generation, const QString &folder,
                         const QByteArray &recipients) -> QByteArray;

  /**
   * @brief The store-relative folder of @p gpgIdFile, as written in and
   * compared against the folder line: `.` for the store root, `/`-separated,
   * from canonical paths, lower-cased on Windows.
   * @param gpgIdFile The `.gpg-id`.
   * @param storeRoot The configured store.
   * @return The folder, or nothing when @p gpgIdFile is not under the store.
   */
  static auto folderOf(const QString &gpgIdFile, const QString &storeRoot)
      -> std::optional<QString>;

  /**
   * @brief The highest generation this device has accepted or written for
   * the `.gpg-id` at @p gpgIdFile. Unreadable is not "never seen": a signed
   * list must then be refused, not compared against 0.
   * @param gpgIdFile The `.gpg-id`.
   * @param error Receives why the record could not be read, if not null.
   * @return 0 when this device has never seen the list, the generation
   *         otherwise; nothing when the record cannot be read.
   */
  static auto remembered(const QString &gpgIdFile, QString *error = nullptr)
      -> std::optional<qint64>;

  /**
   * @brief The outcome of accept(). Only Accepted means the list may be
   * used; the others say why not, and Rollback alone is an authentic list
   * of this folder.
   */
  enum class Verdict {
    /** @brief Folder, generation and record all agree; remembered. */
    Accepted,
    /**
     * @brief Authentic and bound to this folder, but older than a
     * generation this device accepted.
     */
    Rollback,
    /**
     * @brief Older than a generation this device accepted and without a
     * folder line: authentic, but written for who knows which folder
     * (`pass`, or QtPass before 2.0, wrote it), so not this folder's to
     * recover.
     */
    Unbound,
    /**
     * @brief The generation this device accepted last, but not the bytes:
     * two devices saved at once (Git shows that as a conflict), or an
     * authentic list of that generation was swapped in.
     */
    Conflict,
    /** @brief Written for another folder of the store. */
    WrongFolder,
    /** @brief A header line malformed or duplicated. */
    Malformed,
    /** @brief The record could not be read or written. */
    RecordUnavailable,
  };

  /**
   * @brief Decide about a verified list: its folder against where it sits,
   * its generation against the remembered one, its bytes against the
   * remembered digest when the generations are equal, and remember it when
   * it passes. One transaction under the lock: read, compare, write through.
   * @param gpgIdFile The `.gpg-id` the bytes came from.
   * @param contents The verified bytes.
   * @param storeRoot The configured store, for the folder check.
   * @param error Receives the reason for a refusal, if not null. Where the
   *        way through is saving the list again, the text says so, and that
   *        the recipients the dialog then preselects are this list's.
   * @return Accepted when the list may be used; otherwise why not. Without
   *         established freshness state nothing is accepted.
   */
  static auto accept(const QString &gpgIdFile, const QByteArray &contents,
                     const QString &storeRoot, QString *error = nullptr)
      -> Verdict;

  /**
   * @brief Reserve the generation to write next for @p gpgIdFile: one above
   * both what this device remembers and @p verifiedOnDisk, recorded as
   * remembered before it is returned. One transaction under the lock, so two
   * writers in this process cannot get the same number.
   *
   * Reserving before the file is written is what makes a failed write
   * harmless (the record is ahead, the old list stays refused until a save
   * succeeds) and a failed record fatal for the write: a list written but
   * not recorded would let the previous one back in.
   * @param gpgIdFile The `.gpg-id` about to be written.
   * @param verifiedOnDisk The generation of the list currently on disk, if
   *        its signature verified; nothing otherwise. An unverified number
   *        is not taken: planted at the top of the grammar it would exhaust
   *        the counter, planted high it would make every other device see a
   *        jump.
   * @param error Receives why nothing could be reserved, if not null.
   * @return The generation to write, or nothing when the record could not
   *         be read or written or the counter is exhausted; the caller must
   *         not write the list then.
   */
  static auto reserveNext(const QString &gpgIdFile,
                          std::optional<qint64> verifiedOnDisk,
                          QString *error = nullptr) -> std::optional<qint64>;

  /**
   * @brief Record the bytes just written for @p gpgIdFile, so that this
   * device recognises its own list afterwards and refuses another list of
   * the same generation. Called after the file is on disk; the generation
   * in @p contents must be the one reserveNext() gave.
   * @param gpgIdFile The `.gpg-id` just written.
   * @param contents The exact bytes written.
   * @param error Receives why the record could not be written, if not null.
   * @return Whether the digest was recorded. A list written but not
   *         digested is still recognised by its generation; only a second
   *         list of that generation would pass until it is read once.
   */
  static auto recordWritten(const QString &gpgIdFile,
                            const QByteArray &contents,
                            QString *error = nullptr) -> bool;

  /**
   * @brief The digest of a list as the record keeps it: SHA-256 of the
   * exact bytes, hexadecimal, so that two authentic lists of one generation
   * can be told apart.
   * @param contents The bytes.
   * @return 64 hexadecimal characters.
   */
  static auto digest(const QByteArray &contents) -> QString;

  /**
   * @brief The settings key for a `.gpg-id`: a hash of its canonical path,
   * so path spelling (case, separators, links on the way) and QSettings'
   * treatment of `/` do not matter.
   * @param gpgIdFile The `.gpg-id`, existing or about to be written.
   * @return 64 hexadecimal characters.
   */
  static auto key(const QString &gpgIdFile) -> QString;

  /**
   * @brief The file the accepted generations are recorded in, so a refusal
   * can name it: removing it forgets what was accepted, which is the manual
   * way out for a device that cannot save the list itself.
   * @return The record's path.
   */
  static auto recordFile() -> QString;

private:
  /**
   * @brief Whether @p header's folder line, if it has one, names the folder
   * the list sits in: a pair copied elsewhere is that folder's first list,
   * a rollback in disguise.
   * @param gpgIdFile The `.gpg-id` the bytes came from.
   * @param header What its header says.
   * @param storeRoot The configured store.
   * @param error Receives why not, if not null.
   * @return Whether the list belongs where it is.
   */
  static auto boundToItsFolder(const QString &gpgIdFile, const Header &header,
                               const QString &storeRoot, QString *error)
      -> bool;

  /**
   * @brief How a holder of the signing key gets through a refusal, in words:
   * saving the recipients writes the next generation, unless the record is
   * already at the ceiling of the grammar, where it has to be removed first.
   * @param last The generation this device accepted before.
   * @param recordPath The record file, which the text names.
   * @param saving What the user does in the Users dialog, in this refusal's
   *        terms ("checking the recipients and saving").
   * @return The sentence to append to a refusal.
   */
  static auto wayThrough(qint64 last, const QString &recordPath,
                         const QString &saving) -> QString;

  /**
   * @brief The refusal for a list older than the one accepted here: a
   * rollback, or an unbound list that could be any folder's history.
   * @param gpgIdFile The `.gpg-id` the bytes came from.
   * @param header What its header says.
   * @param last The generation this device accepted before.
   * @param recordPath The record file, which the text names.
   * @param error Receives the reason, if not null.
   * @return Rollback for a list that says which folder it is for, Unbound
   *         for one that does not.
   */
  static auto staleList(const QString &gpgIdFile, const Header &header,
                        qint64 last, const QString &recordPath, QString *error)
      -> Verdict;
};

#endif // SRC_GPGIDGENERATION_H_
