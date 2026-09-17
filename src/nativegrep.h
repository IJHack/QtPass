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
   * @brief Ask running searches to stop without waiting for them.
   */
  void cancel();

  /**
   * @brief Decrypt one .gpg file and return its trimmed lines matching @p rx.
   * @param env Environment for gpg.
   * @param gpgExe The gpg executable.
   * @param filePath The encrypted file.
   * @param rx The pattern.
   * @return Matching lines; empty when gpg fails or nothing matches.
   */
  static auto matchFile(const QProcessEnvironment &env, const QString &gpgExe,
                        const QString &filePath, const QRegularExpression &rx)
      -> QStringList;
  /**
   * @brief Walk @p storeDir, decrypt every .gpg file and collect the matches.
   * @param env Environment for gpg.
   * @param gpgExe The gpg executable.
   * @param storeDir The store.
   * @param rx The pattern.
   * @return (entry name without .gpg, matching lines) per file with matches;
   *         empty when the current thread is asked to stop.
   */
  static auto scanStore(const QProcessEnvironment &env, const QString &gpgExe,
                        const QString &storeDir, const QRegularExpression &rx)
      -> QList<QPair<QString, QStringList>>;

signals:
  /**
   * @brief The newest search is over.
   * @param results (entry, matching lines) pairs.
   */
  void finished(const QList<QPair<QString, QStringList>> &results);

private:
  int m_seq = 0;
  QList<QThread *> m_threads;
};

#endif // SRC_NATIVEGREP_H_
