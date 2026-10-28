// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "nativegrep.h"
#include "executor.h"
#include "pass.h"
#include "util.h"
#include <QDir>
#include <QElapsedTimer>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStringTokenizer>
#include <QThread>
#include <utility>

NativeGrep::NativeGrep(QObject *parent) : QObject(parent) {}

NativeGrep::~NativeGrep() {
  static constexpr int kThreadTimeoutMs = 5000;
  cancel();
  QElapsedTimer elapsed;
  elapsed.start();
  for (const Worker &w : std::as_const(m_workers)) {
    if (w.thread && w.thread->isRunning()) {
      const int remaining =
          kThreadTimeoutMs - static_cast<int>(elapsed.elapsed());
      if (remaining > 0)
        w.thread->wait(remaining);
    }
  }
}

/**
 * @brief Decrypt one .gpg file and return lines matching rx.
 */
auto NativeGrep::matchFile(const QProcessEnvironment &env,
                           const QString &gpgExe, const QString &filePath,
                           const QRegularExpression &rx,
                           const std::atomic_bool *cancel) -> QStringList {
  const QString translatedPath =
      Executor::translatePathForWsl(filePath, gpgExe);
  QString plaintext;
  // The QProcess overload is the one that takes the cancel flag; it polls
  // the flag while waiting and terminates (then kills) gpg from this thread,
  // the one that owns the process.
  QProcess gpg;
  gpg.setProcessEnvironment(env);
  const int rc = Executor::executeBlocking(
      gpg, gpgExe,
      QStringList{"-d", "--quiet", "--yes", "--no-encrypt-to", "--batch",
                  "--use-agent"} +
          Pass::maxOutputArgs() + QStringList{translatedPath},
      QString(), &plaintext, nullptr, cancel);
  if (rc != 0 || plaintext.isEmpty())
    return {};
  // Line by line over the text, never a list of every line (a compressed
  // entry can expand into tens of millions), and a bounded number taken.
  QStringList matches;
  for (QStringView line : QStringView(plaintext).tokenize(u'\n')) {
    if (line.endsWith(u'\r'))
      line.chop(1);
    const QStringView t = line.trimmed();
    if (t.isEmpty())
      continue;
    const QRegularExpressionMatch m = rx.matchView(line);
    if (!m.hasMatch())
      continue;
    if (matches.size() == kMaxMatchesPerEntry) {
      matches << QString(); // isMoreMarker(): not a match, not counted
      break;
    }
    if (t.size() <= kMaxPreviewLength) {
      matches << t.toString();
      continue;
    }
    // A window of the line that holds the match, a little context before
    // it; "…" marks what was left out on either side.
    const qsizetype at =
        qMax<qsizetype>(0, m.capturedStart() - (t.data() - line.data()));
    const qsizetype start = qBound<qsizetype>(0, at - kMaxPreviewLength / 4,
                                              t.size() - kMaxPreviewLength);
    QString preview = t.mid(start, kMaxPreviewLength).toString();
    if (start > 0)
      preview.prepend(QChar(0x2026));
    if (start + kMaxPreviewLength < t.size())
      preview.append(QChar(0x2026));
    matches << preview;
  }
  return matches;
}

/**
 * @brief Walk the store, decrypt every .gpg file, collect matches.
 */
auto NativeGrep::scanStore(const QProcessEnvironment &env,
                           const QString &gpgExe, const QString &storeDir,
                           const QRegularExpression &rx,
                           const std::atomic_bool *cancel)
    -> QList<QPair<QString, QStringList>> {
  QList<QPair<QString, QStringList>> results;
  int total = 0; // matching lines, the markers not counted
  // Regular files only: a link or junction is not an entry, and searching
  // through one would decrypt files outside the store.
  const QStringList files =
      Util::regularFilesUnder(storeDir, QStringList() << "*.gpg");
  for (const QString &filePath : files) {
    if (QThread::currentThread()->isInterruptionRequested() ||
        (cancel != nullptr && cancel->load()))
      return {};
    if (total == kMaxMatches) {
      // The budget is spent and entries remain: the search stopped here.
      results.append({QString(), {}});
      break;
    }
    QStringList matches = matchFile(env, gpgExe, filePath, rx, cancel);
    if (matches.isEmpty())
      continue;
    const bool more = isMoreMarker(matches.constLast());
    if (more)
      matches.removeLast();
    // The store-wide budget: what still fits, and a marker when that cut
    // the entry short.
    const int room = kMaxMatches - total;
    if (matches.size() > room) {
      matches = matches.mid(0, room);
      matches << QString();
    } else if (more) {
      matches << QString();
    }
    total += static_cast<int>(matches.size()) -
             (isMoreMarker(matches.constLast()) ? 1 : 0);
    QString entry = QDir(storeDir).relativeFilePath(filePath);
    if (entry.endsWith(QLatin1String(".gpg")))
      entry.chop(4);
    results.append({entry, matches});
  }
  return results;
}

/**
 * @brief Start a search on a worker thread.
 *
 * Results are emitted on the owner's thread via QMetaObject::invokeMethod. A
 * sequence counter discards results from superseded searches; the previous
 * search is asked to stop but not waited for, since blocking the UI thread
 * while gpg decrypts would freeze the interface.
 */
void NativeGrep::search(const QString &pattern, bool caseInsensitive,
                        const QString &gpgExe, const QString &storeDir,
                        const QProcessEnvironment &env) {
  cancel();

  // Advance the sequence before any early return so in-flight workers from the
  // previous query fail the seq check and cannot publish stale results.
  const int seq = ++m_seq;

  // Use trimmed() rather than isEmpty(): a whitespace-only string is a valid
  // regex that matches every non-empty line, which is almost never intentional
  // and would decrypt the entire store.
  //
  // Both early returns post finished() via Qt::QueuedConnection so that the
  // signal is always delivered asynchronously after search() returns, matching
  // the contract of the threaded path.
  if (pattern.trimmed().isEmpty()) {
    QMetaObject::invokeMethod(
        this,
        [this, seq]() {
          if (m_seq == seq)
            emit finished({});
        },
        Qt::QueuedConnection);
    return;
  }

  const QRegularExpression rx(
      pattern, caseInsensitive ? QRegularExpression::CaseInsensitiveOption
                               : QRegularExpression::PatternOptions{});
  if (!rx.isValid()) {
    QMetaObject::invokeMethod(
        this,
        [this, seq]() {
          if (m_seq == seq)
            emit finished({});
        },
        Qt::QueuedConnection);
    return;
  }
  QPointer<NativeGrep> self(this);

  auto emitResults = [self, seq](QList<QPair<QString, QStringList>> results) {
    if (!self)
      return;
    QMetaObject::invokeMethod(
        self,
        [self, seq, results = std::move(results)]() {
          if (self && self->m_seq == seq)
            emit self->finished(results);
        },
        Qt::QueuedConnection);
  };

  auto cancelFlag = std::make_shared<std::atomic_bool>(false);
  QThread *thread = QThread::create([gpgExe, storeDir, env, rx, cancelFlag,
                                     emitResults = std::move(emitResults)]() {
    std::move(emitResults)(
        scanStore(env, gpgExe, storeDir, rx, cancelFlag.get()));
  });

  m_workers.append({thread, cancelFlag});
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  connect(thread, &QThread::finished, this, [this, thread]() {
    m_workers.removeIf(
        [thread](const Worker &w) { return w.thread == thread; });
  });
  thread->start();
}

void NativeGrep::cancel() {
  for (const Worker &w : std::as_const(m_workers)) {
    w.cancel->store(true);
    if (w.thread && w.thread->isRunning())
      w.thread->requestInterruption();
  }
}
