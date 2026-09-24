// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TESTS_AUTO_TESTPASS_H_
#define TESTS_AUTO_TESTPASS_H_

#include "../../src/pass.h"

#include <QElapsedTimer>
#include <QtTest>

/**
 * @brief A Pass that does nothing, for suites that need a backend object
 * but not a backend: derive from it and override only the operations the
 * test records or answers.
 *
 * One place to stub the pure virtuals, so a new one in Pass means one edit
 * here rather than one in every suite that fakes a backend.
 */
class NullPass : public Pass {
public:
  NullPass() { init(AppSettings()); }
  void GitInit() override {}
  void GitPull() override {}
  void GitPull_b() override {}
  void GitPush() override {}
  void Show(QString) override {}
  void Insert(QString, QString, bool) override {}
  void Remove(QString, bool) override {}
  void Move(const QString, const QString, const bool) override {}
  void Copy(const QString, const QString, const bool) override {}
  void Init(QString, const QList<UserInfo> &) override {}
  void Grep(QString, bool) override {}
};

/**
 * @brief QTRY_VERIFY_WITH_TIMEOUT for a helper that returns a value:
 * poll @p expr every 20 ms for up to @p timeout ms, and on timeout record
 * the failure and return @p ret from the enclosing function.
 */
#define QTRY_VERIFY_WITH_TIMEOUT_RETURN(expr, timeout, ret)                    \
  do {                                                                         \
    QElapsedTimer timer;                                                       \
    timer.start();                                                             \
    while (!(expr) && timer.elapsed() < (timeout)) {                           \
      QTest::qWait(20);                                                        \
    }                                                                          \
    if (!(expr)) {                                                             \
      QTest::qFail(#expr, __FILE__, __LINE__);                                 \
      return ret;                                                              \
    }                                                                          \
  } while (false)

/**
 * @brief A two-key `gpg --with-colons` listing (Alice, ultimately trusted,
 * and Bob, fully trusted) for a stand-in gpg, so a recipients dialog has
 * keys to offer without a real keyring.
 */
inline constexpr char kColonListing[] =
    "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
    "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
    "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
    "<alice@example.org>::::::::::0:\n"
    "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
    "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
    "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Bob "
    "<bob@example.org>::::::::::0:\n";

#endif // TESTS_AUTO_TESTPASS_H_
