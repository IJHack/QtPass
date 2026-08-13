// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include "../../../src/appsettings.h"
#include "../../../src/filecontent.h"
#include "../../../src/otpcodewidget.h"
#include "../../../src/passworddisplaypanel.h"
#include "../../../src/qpushbuttonwithclipboard.h"

class tst_passworddisplaypanel : public QObject {
  Q_OBJECT

  QWidget *m_parent = nullptr;
  QGridLayout *m_grid = nullptr;
  QVBoxLayout *m_container = nullptr;
  PasswordDisplayPanel *m_panel = nullptr;

private Q_SLOTS:
  void init();
  void cleanup();
  void displayFieldsAddsRows();
  void displayFieldsSkipsEmptyPassword();
  void clearRemovesAllRows();
  void appendFieldAddsOneRow();
  void otpFieldRendersLiveCodeInPlace();
  void otpFieldNeverShowsTheSecret();
  void otpFieldSuppressedWhenSupportDisabled();
  void otpConfigFromBodyIsAppended();
  void invalidOtpConfigRendersPlaceholder();
  void otpCodeMatchesRfcVectorForPinnedTime();
  void otpCopyButtonEmitsCurrentCode();
  void otpRowHonoursClipboardNever();
  void otpRowSurvivesClearWithoutCrashing();
  void duplicateOtpFieldsRenderOneRow();
  void currentOtpCodeReturnsDisplayedCode();
  void currentOtpCodeEmptyWithoutOtpRow();
  void currentOtpCodeEmptyAfterClear();
  void otpUriAsPasswordIsNeverRendered();
  void otpUriInDifferentlyNamedFieldIsNeverRendered();

private:
  [[nodiscard]] auto otpWidgetAt(int row) const -> OtpCodeWidget *;
};

void tst_passworddisplaypanel::init() {
  m_parent = new QWidget;
  m_grid = new QGridLayout;
  m_container = new QVBoxLayout(m_parent);
  m_container->addLayout(m_grid);
  m_panel = new PasswordDisplayPanel(m_grid, m_container, m_parent);
}

void tst_passworddisplaypanel::cleanup() {
  delete m_panel;
  m_panel = nullptr;
  delete m_parent;
  m_parent = nullptr;
  m_grid = nullptr;
  m_container = nullptr;
}

void tst_passworddisplaypanel::displayFieldsAddsRows() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"url", "https://example.org"}}, s);
  // Two fields (password + url), each a label + value widget => 4 grid items.
  QCOMPARE(m_grid->count(), 4);
}

void tst_passworddisplaypanel::displayFieldsSkipsEmptyPassword() {
  AppSettings s;
  m_panel->displayFields(QString(), NamedValues{}, s);
  QVERIFY2(m_grid->count() == 0,
           "An empty password with no fields must leave the grid empty");
}

void tst_passworddisplaypanel::clearRemovesAllRows() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"), NamedValues{}, s);
  QVERIFY2(m_grid->count() > 0, "precondition: grid populated");
  m_panel->clear();
  QVERIFY2(m_grid->count() == 0, "clear() must remove every grid row");
}

void tst_passworddisplaypanel::appendFieldAddsOneRow() {
  AppSettings s;
  const int before = m_grid->count();
  m_panel->appendField(QStringLiteral("OTP Code"), QStringLiteral("123456"), s);
  QCOMPARE(m_grid->count(), before + 2);
}

/// RFC 6238 appendix B seed, so a pinned time has a published expected code.
static const QString kOtpUri = QStringLiteral(
    "otpauth://totp/Example:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&"
    "issuer=Example&digits=6&period=30");
/// The base32 secret embedded in kOtpUri, used for leak assertions.
static const QString kOtpSecret =
    QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");

auto tst_passworddisplaypanel::otpWidgetAt(int row) const -> OtpCodeWidget * {
  QLayoutItem *item = m_grid->itemAtPosition(row, 1);
  if (item == nullptr || item->widget() == nullptr) {
    return nullptr;
  }
  return item->widget()->findChild<OtpCodeWidget *>();
}

/**
 * @brief Collect everything the grid exposes to the user for a given widget.
 *
 * Must cover every sink addField() writes a value into, not just QLabel:
 * field values go into a QTextBrowser (or a QLineEdit for the password) and the
 * copy payload lives inside QPushButtonWithClipboard. An earlier version read
 * only QLabel::text() and toolTip(), which made the "secret is never shown"
 * assertions pass even with the suppression removed entirely.
 */
static auto widgetText(const QWidget *widget) -> QString {
  QString text = widget->toolTip();
  if (const auto *label = qobject_cast<const QLabel *>(widget)) {
    text += label->text();
  } else if (const auto *browser = qobject_cast<const QTextBrowser *>(widget)) {
    text += browser->toPlainText();
    text += browser->toHtml();
  } else if (const auto *edit = qobject_cast<const QLineEdit *>(widget)) {
    text += edit->text();
  } else if (const auto *copy =
                 qobject_cast<const QPushButtonWithClipboard *>(widget)) {
    text += copy->getTextToCopy();
  }
  return text;
}

/// Collect every string the grid renders or would hand to the clipboard.
static auto renderedText(QGridLayout *grid) -> QString {
  QString text;
  for (int i = 0; i < grid->count(); ++i) {
    QWidget *widget = grid->itemAt(i)->widget();
    if (widget == nullptr) {
      continue;
    }
    text += widgetText(widget);
    const QList<QWidget *> all = widget->findChildren<QWidget *>();
    for (const QWidget *child : all) {
      text += widgetText(child);
    }
  }
  return text;
}

void tst_passworddisplaypanel::otpFieldRendersLiveCodeInPlace() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  // Password plus one OTP row: still exactly two grid items per row.
  QCOMPARE(m_grid->count(), 4);
  QVERIFY2(otpWidgetAt(1) != nullptr,
           "the OTP row must render an OtpCodeWidget in the field's position");
}

void tst_passworddisplaypanel::otpFieldNeverShowsTheSecret() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  const QString text = renderedText(m_grid);
  QVERIFY2(!text.contains(kOtpSecret), "the shared secret must never be shown");
  QVERIFY2(!text.contains(QStringLiteral("otpauth")),
           "the otpauth URI must never be shown");
}

/**
 * @brief With OTP support off MainWindow passes an empty config; the field must
 * still be suppressed rather than falling through to a verbatim row.
 */
void tst_passworddisplaypanel::otpFieldSuppressedWhenSupportDisabled() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, QString());
  // Only the password row remains.
  QCOMPARE(m_grid->count(), 2);
  const QString text = renderedText(m_grid);
  QVERIFY2(!text.contains(kOtpSecret), "the shared secret must never be shown");
}

void tst_passworddisplaypanel::otpConfigFromBodyIsAppended() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"url", "https://example.org"}}, s,
                         kOtpUri);
  // Password, url, then the appended OTP row.
  QCOMPARE(m_grid->count(), 6);
  QVERIFY2(otpWidgetAt(2) != nullptr,
           "a body-sourced OTP row must be appended after the named fields");
}

void tst_passworddisplaypanel::invalidOtpConfigRendersPlaceholder() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", QStringLiteral("not base32!")}}, s,
                         QStringLiteral("not base32!"));
  QCOMPARE(m_grid->count(), 4);
  QVERIFY2(otpWidgetAt(1) == nullptr,
           "an unusable configuration must not produce a live code widget");

  QWidget *value = m_grid->itemAtPosition(1, 1)->widget();
  QVERIFY(value != nullptr);
  QVERIFY2(value->findChild<QPushButtonWithClipboard *>() == nullptr,
           "the placeholder must not be offered as clipboard content");
}

/**
 * @brief refresh() takes an explicit time so the code is deterministic instead
 * of racing the wall clock.
 */
void tst_passworddisplaypanel::otpCodeMatchesRfcVectorForPinnedTime() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  OtpCodeWidget *otp = otpWidgetAt(1);
  QVERIFY(otp != nullptr);

  otp->refresh(1234567890ULL);
  QCOMPARE(otp->code(), QStringLiteral("005924"));
  QCOMPARE(otp->secondsRemaining(), 30U - (1234567890U % 30U));

  auto *progress = otp->findChild<QProgressBar *>();
  QVERIFY(progress != nullptr);
  QCOMPARE(progress->maximum(), 30);
  QCOMPARE(progress->value(), static_cast<int>(otp->secondsRemaining()));

  // A different period must produce a different code.
  otp->refresh(1111111109ULL);
  QCOMPARE(otp->code(), QStringLiteral("081804"));
}

void tst_passworddisplaypanel::otpCopyButtonEmitsCurrentCode() {
  AppSettings s;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  OtpCodeWidget *otp = otpWidgetAt(1);
  QVERIFY(otp != nullptr);
  otp->refresh(1234567890ULL);

  auto *button = otp->findChild<QPushButtonWithClipboard *>();
  QVERIFY2(button != nullptr,
           "a copy button is expected unless the clipboard is disabled");

  QSignalSpy spy(m_panel, &PasswordDisplayPanel::copyRequested);
  button->click();
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), otp->code());
  QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("005924"));
}

/// CLIPBOARD_NEVER must not put the code on a copy button at all.
void tst_passworddisplaypanel::otpRowHonoursClipboardNever() {
  AppSettings s;
  s.clipBoardType = Enums::CLIPBOARD_NEVER;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  OtpCodeWidget *otp = otpWidgetAt(1);
  QVERIFY(otp != nullptr);
  QVERIFY(otp->findChild<QPushButtonWithClipboard *>() == nullptr);
}

/**
 * @brief The refresh timer is a child of the widget, so clear()'s delete must
 * stop it. A surviving timer would fire into a destroyed label.
 */
void tst_passworddisplaypanel::otpRowSurvivesClearWithoutCrashing() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  QVERIFY(otpWidgetAt(1) != nullptr);

  m_panel->clear();
  QCOMPARE(m_grid->count(), 0);
  // Longer than the one second refresh interval.
  QTest::qWait(1200);
  QCOMPARE(m_grid->count(), 0);
}

void tst_passworddisplaypanel::duplicateOtpFieldsRenderOneRow() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}, {"TOTP", kOtpUri}}, s,
                         kOtpUri);
  // Password plus a single OTP row; the duplicate leaves no empty row behind.
  QCOMPARE(m_grid->count(), 4);
  QVERIFY(otpWidgetAt(1) != nullptr);
}

/**
 * @brief MainWindow::onOtp copies the code the user can see rather than
 * decrypting the entry again, so the panel must hand it out.
 */
void tst_passworddisplaypanel::currentOtpCodeReturnsDisplayedCode() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  OtpCodeWidget *otp = otpWidgetAt(1);
  QVERIFY(otp != nullptr);

  otp->refresh(1234567890ULL);
  QCOMPARE(m_panel->currentOtpCode(), QStringLiteral("005924"));
  QCOMPARE(m_panel->currentOtpCode(), otp->code());

  // Must track the live code, not a snapshot taken when the row was built.
  otp->refresh(1111111109ULL);
  QCOMPARE(m_panel->currentOtpCode(), QStringLiteral("081804"));
}

void tst_passworddisplaypanel::currentOtpCodeEmptyWithoutOtpRow() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"url", "https://example.org"}}, s);
  QVERIFY2(m_panel->currentOtpCode().isEmpty(),
           "an entry with no OTP must report no code");

  // Also when the field exists but OTP support is off.
  m_panel->clear();
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, QString());
  QVERIFY(m_panel->currentOtpCode().isEmpty());
}

void tst_passworddisplaypanel::currentOtpCodeEmptyAfterClear() {
  AppSettings s;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"OTP", kOtpUri}}, s, kOtpUri);
  QVERIFY(!m_panel->currentOtpCode().isEmpty());

  // The panel autoclear timer wipes the row; there is then nothing to copy.
  m_panel->clear();
  QVERIFY(m_panel->currentOtpCode().isEmpty());
}

/**
 * @brief `pass otp insert` writes the URI as the entry's only line, so it
 * arrives as the "password". It is a shared secret and must not be rendered or
 * offered for copying.
 */
void tst_passworddisplaypanel::otpUriAsPasswordIsNeverRendered() {
  AppSettings s;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  // Defence in depth: FileContent::getPasswordForDisplay() already blanks it,
  // but the panel must not render a password line that is a shared secret even
  // if one reaches it.
  m_panel->displayFields(kOtpUri, NamedValues{}, s, kOtpUri);

  const QString text = renderedText(m_grid);
  QVERIFY2(!text.contains(kOtpSecret), "the shared secret must never be shown");
  QVERIFY2(!text.contains(QStringLiteral("otpauth")),
           "the otpauth URI must never be shown");
  QVERIFY2(otpWidgetAt(1) != nullptr,
           "the entry still has a usable OTP configuration");
}

/**
 * @brief A field holding an otpauth URI is a secret whatever it is called;
 * suppression keyed only on the name leaked `2fa:`, `mfa:` and friends.
 */
void tst_passworddisplaypanel::otpUriInDifferentlyNamedFieldIsNeverRendered() {
  AppSettings s;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"2fa", kOtpUri}}, s, kOtpUri);

  const QString text = renderedText(m_grid);
  QVERIFY2(!text.contains(kOtpSecret), "the shared secret must never be shown");
  QVERIFY2(!text.contains(QStringLiteral("otpauth")),
           "the otpauth URI must never be shown");
}

QTEST_MAIN(tst_passworddisplaypanel)
#include "tst_passworddisplaypanel.moc"
