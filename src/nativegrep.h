// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_NATIVEGREP_H_
#define SRC_NATIVEGREP_H_

#include <QList>
#include <QObject>
#include <QPair>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>

class QRegularExpression;
class QThread;

/**
 * @class NativeGrep
 * @brief Full-text search over a password store without `pass grep`: every
 * `.gpg` file is decrypted with gpg on a worker thread and its lines matched
 * against a QRegularExpression. Only the newest search may report; a search
 * started while another runs asks that one to stop and supersedes it.
 */
class NativeGrep : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Create a searcher.
   * @param parent Owner; results are delivered on its thread.
   */
  explicit NativeGrep(QObject *parent = nullptr);

  /**
   * @brief Most matching lines taken from one entry.
   *
   * A compressed entry can expand into millions of lines within the
   * plaintext limit, each a result and a row in the results list; past this
   * many, the entry gets one line saying there are more.
   */
  static constexpr int kMaxMatchesPerEntry = 100;

  /**
   * @brief Most matching lines a search collects over the whole store.
   *
   * The search stops once it has this many; the window says so.
   */
  static constexpr int kMaxMatches = 10000;

  /**
   * @brief Longest preview of a matching line, in characters.
   *
   * A result shows where the pattern matched, not the line; a single line
   * can be most of an entry's 64 MiB plaintext, and 10000 such copies would
   * not fit in memory. A longer line is shown as a window of this many
   * characters around the match, "…" where the line goes on.
   */
  static constexpr qsizetype kMaxPreviewLength = 200;

  /**
   * @brief Whether a line in a result is the marker for "more matches in
   * this entry" rather than a match.
   *
   * A match is a trimmed, non-empty line, so an empty one can only be the
   * marker, which matchFile() and scanStore() put last. Not a match: not
   * counted, not content to hide.
   * @param line A line of a result.
   * @return Whether it is the marker.
   */
  [[nodiscard]] static auto isMoreMarker(const QString &line) -> bool {
    return line.isEmpty();
  }

  /**
   * @brief Whether a result is the marker for "the search stopped at
   * kMaxMatches, more entries were not searched".
   *
   * No entry has an empty name, so a result without one can only be the
   * marker, which scanStore() puts last.
   * @param result A result.
   * @return Whether it is the marker.
   */
  [[nodiscard]] static auto
  isStoppedMarker(const QPair<QString, QStringList> &result) -> bool {
    return result.first.isEmpty();
  }
  /**
   * @brief Interrupt running searches and wait up to five seconds for them.
   */
  ~NativeGrep() override;

  /**
   * @brief Start a search; finished() follows asynchronously, also for an
   * empty or invalid pattern (with no results).
   * @param pattern PCRE pattern.
   * @param caseInsensitive Match regardless of case.
   * @param gpgExe The gpg executable (a `wsl ` prefix routes paths through
   *        wslpath).
   * @param storeDir The store to walk.
   * @param env Environment for gpg (GNUPGHOME and friends).
   */
  void search(const QString &pattern, bool caseInsensitive,
              const QString &gpgExe, const QString &storeDir,
              const QProcessEnvironment &env);

  /**
   * @brief Ask running searches to stop without waiting for them: the walk
   * ends at the next file and a gpg that is decrypting is terminated by its
   * own worker thread.
   */
  void cancel();

  /**
   * @brief Decrypt one .gpg file and return its trimmed lines matching @p rx.
   * @param env Environment for gpg.
   * @param gpgExe The gpg executable.
   * @param filePath The encrypted file.
   * @param rx The pattern.
   * @param cancel When set, gpg is not started, or is terminated while it
   *        runs; nullptr to run to completion.
   * @return Matching lines, at most kMaxMatchesPerEntry and then a marker
   *         (isMoreMarker()) when there are more; empty when gpg fails, is
   *         cancelled or nothing matches.
   */
  static auto matchFile(const QProcessEnvironment &env, const QString &gpgExe,
                        const QString &filePath, const QRegularExpression &rx,
                        const std::atomic_bool *cancel = nullptr)
      -> QStringList;
  /**
   * @brief Walk @p storeDir, decrypt every .gpg file and collect the matches.
   * @param env Environment for gpg.
   * @param gpgExe The gpg executable.
   * @param storeDir The store.
   * @param rx The pattern.
   * @param cancel Stops the walk and the running gpg when set; nullptr to
   *        rely on QThread::isInterruptionRequested() alone.
   * @return (entry name without .gpg, matching lines) per file with matches,
   *         at most kMaxMatches lines in all, an entry cut short ending in a
   *         marker (isMoreMarker()); when the walk stopped at that budget
   *         with entries left, a last result that is a marker
   *         (isStoppedMarker()). Empty when the search is stopped.
   */
  static auto scanStore(const QProcessEnvironment &env, const QString &gpgExe,
                        const QString &storeDir, const QRegularExpression &rx,
                        const std::atomic_bool *cancel = nullptr)
      -> QList<QPair<QString, QStringList>>;

signals:
  /**
   * @brief The newest search is over.
   * @param results (entry, matching lines) pairs.
   */
  void finished(const QList<QPair<QString, QStringList>> &results);

private:
  /// A running search: its thread and the flag its gpg runs poll. The flag
  /// is shared with the thread's lambda so it outlives this object if a
  /// search is still winding down when it is destroyed.
  struct Worker {
    QThread *thread;
    std::shared_ptr<std::atomic_bool> cancel;
  };
  int m_seq = 0;
  QList<Worker> m_workers;
};

#endif // SRC_NATIVEGREP_H_
