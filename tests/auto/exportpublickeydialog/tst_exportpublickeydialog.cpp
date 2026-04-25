// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtTest>

#include "../../../src/exportpublickeydialog.h"

class tst_exportpublickeydialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void sanitizeKeyId_data();
  void sanitizeKeyId();
  void labelShowsKeyIdAsPlainText();
  void plainTextEditShowsArmoredKey();
  void copyButtonCopiesToClipboardAndRelabels();
};

void tst_exportpublickeydialog::sanitizeKeyId_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<QString>("expected");
  QTest::newRow("hex-fingerprint") << QStringLiteral("DEADBEEFCAFE0123")
                                   << QStringLiteral("DEADBEEFCAFE0123");
  QTest::newRow("multiple-ids")
      << QStringLiteral("DEADBEEF FEEDFACE") << QStringLiteral("DEADBEEF");
  QTest::newRow("path-separator")
      << QStringLiteral("DEAD/BEEF") << QStringLiteral("DEADBEEF");
  QTest::newRow("relative-traversal")
      << QStringLiteral("../etc") << QStringLiteral("etc");
  QTest::newRow("empty-input") << QString() << QString();
  QTest::newRow("only-unsafe-chars") << QStringLiteral("@#$%") << QString();
  QTest::newRow("dash-and-underscore-allowed")
      << QStringLiteral("DEAD_BEEF-1234") << QStringLiteral("DEAD_BEEF-1234");
  QTest::newRow("leading-whitespace")
      << QStringLiteral("   DEADBEEF") << QString();
}

void tst_exportpublickeydialog::sanitizeKeyId() {
  QFETCH(QString, input);
  QFETCH(QString, expected);
  QCOMPARE(ExportPublicKeyDialog::sanitizeKeyIdForFilename(input), expected);
}

void tst_exportpublickeydialog::labelShowsKeyIdAsPlainText() {
  // Use HTML-looking content to confirm it isn't rendered as rich text.
  const QString keyId = QStringLiteral("<b>DEADBEEF</b>");
  ExportPublicKeyDialog dialog(keyId, QStringLiteral("armored"));

  auto *label = dialog.findChild<QLabel *>(QStringLiteral("keyIdLabel"));
  QVERIFY(label != nullptr);
  QCOMPARE(label->textFormat(), Qt::PlainText);
  QVERIFY(label->text().contains(keyId));
}

void tst_exportpublickeydialog::plainTextEditShowsArmoredKey() {
  const QString armored =
      QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                     "-----END PGP PUBLIC KEY BLOCK-----\n");
  ExportPublicKeyDialog dialog(QStringLiteral("DEADBEEF"), armored);

  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY(edit != nullptr);
  QCOMPARE(edit->toPlainText(), armored);
  QVERIFY(edit->isReadOnly());
}

void tst_exportpublickeydialog::copyButtonCopiesToClipboardAndRelabels() {
  // QClipboard may not be available on every CI platform; skip cleanly when
  // it isn't rather than failing.
  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard == nullptr) {
    QSKIP("No clipboard available on this platform");
  }

  const QString armored = QStringLiteral("clipboard-test-payload");
  ExportPublicKeyDialog dialog(QStringLiteral("DEADBEEF"), armored);
  auto *button = dialog.findChild<QPushButton *>(QStringLiteral("copyButton"));
  QVERIFY(button != nullptr);
  const QString originalText = button->text();
  QVERIFY(!originalText.isEmpty());

  // Trigger the click handler directly to avoid relying on event delivery
  // and the platform's window manager.
  QMetaObject::invokeMethod(&dialog, "on_copyButton_clicked",
                            Qt::DirectConnection);

  QCOMPARE(clipboard->text(), armored);
  QCOMPARE(button->text(), QStringLiteral("Copied!"));

  // The relabel reverts via QTimer::singleShot(1500, ...). Wait a little
  // longer than that for the timer to fire.
  QTRY_COMPARE_WITH_TIMEOUT(button->text(), originalText, 4000);
}

QTEST_MAIN(tst_exportpublickeydialog)
#include "tst_exportpublickeydialog.moc"
