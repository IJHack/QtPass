// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "../../../src/exportpublickeydialog.h"
#include "../testsettings.h"

class tst_exportpublickeydialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void sanitizeKeyId_data();
  void sanitizeKeyId();
  void labelShowsKeyIdAsPlainText();
  void plainTextEditShowsArmoredKey();
  void copyButtonCopiesToClipboardAndRelabels();
  void saveButtonWritesKeyToChosenFile();
  void saveButtonCancelWritesNothing();
  void saveButtonWarnsWhenFileCannotBeOpened();

private:
  static auto driveSaveDialog(ExportPublicKeyDialog *dialog,
                              const QString &targetPath, QString *proposedPath,
                              QString *warningText) -> QString;
};

void tst_exportpublickeydialog::initTestCase() {
  isolateTestSettings();
  // The Save tests drive the Qt widget file dialog and message box through
  // their QWidget API; a native (platform theme) dialog has neither, so pin
  // the widget versions even when the suite runs outside --platform
  // offscreen.
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
}

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
      << QStringLiteral("   DEADBEEF") << QStringLiteral("DEADBEEF");
  QTest::newRow("tab-separated")
      << QStringLiteral("DEADBEEF\tFEEDFACE") << QStringLiteral("DEADBEEF");
  QTest::newRow("only-whitespace") << QStringLiteral("  \t\n ") << QString();
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
  // QClipboard may not be functional on every CI platform (e.g., minimal QPA);
  // perform a round-trip test to verify clipboard capability before proceeding.
  QClipboard *clipboard = QApplication::clipboard();
  const QString originalClipboard = clipboard->text();
  // RAII restore so the user's clipboard is reset on every exit path
  // (early QSKIP, normal completion, or QVERIFY/QCOMPARE failure).
  const auto restoreClipboard = qScopeGuard([clipboard, originalClipboard]() {
    clipboard->setText(originalClipboard);
  });
  const QString probeString = QStringLiteral("__qtpass_clipboard_probe__");
  clipboard->setText(probeString);
  if (clipboard->text() != probeString) {
    QSKIP("Clipboard is not functional on this platform");
  }

  const QString armored = QStringLiteral("clipboard-test-payload");
  ExportPublicKeyDialog dialog(QStringLiteral("DEADBEEF"), armored);
  auto *button = dialog.findChild<QPushButton *>(QStringLiteral("copyButton"));
  QVERIFY(button != nullptr);
  const QString originalText = button->text();
  QVERIFY(!originalText.isEmpty());

  button->click();

  QCOMPARE(clipboard->text(), armored);
  QCOMPARE(button->text(), QStringLiteral("Copied!"));

  // The relabel reverts via QTimer::singleShot(1500, ...). Wait a little
  // longer than that for the timer to fire.
  QTRY_COMPARE_WITH_TIMEOUT(button->text(), originalText, 4000);
}

/**
 * @brief Click the Save button and drive the modal QFileDialog it opens
 *        from a timer, the way a user would: record the path the dialog
 *        proposes, then either accept @p targetPath or cancel when it is
 *        empty. Any QMessageBox that follows is accepted and its text
 *        captured. A dialog that is still open after its drive ran is
 *        rejected so the test fails instead of hanging the suite.
 * @return Empty on success, otherwise what went wrong.
 */
auto tst_exportpublickeydialog::driveSaveDialog(ExportPublicKeyDialog *dialog,
                                                const QString &targetPath,
                                                QString *proposedPath,
                                                QString *warningText)
    -> QString {
  auto *button = dialog->findChild<QPushButton *>(QStringLiteral("saveButton"));
  if (button == nullptr) {
    return QStringLiteral("no saveButton child in ExportPublicKeyDialog");
  }
  int fileDialogs = 0;
  QStringList stuck;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    QWidget *modal = QApplication::activeModalWidget();
    if (modal == nullptr) {
      return;
    }
    if (modal->property("tst_driven").toBool()) {
      if (modal->property("tst_pending").toBool()) {
        return; // the queued accept() below has not run yet
      }
      // accept()/reject() hide a dialog synchronously, so meeting a driven
      // modal again means the drive did not close it (e.g. QFileDialog
      // refused the name). Reject it rather than let click() block forever.
      stuck << QString::fromLatin1(modal->metaObject()->className());
      if (auto *stuckDialog = qobject_cast<QDialog *>(modal)) {
        stuckDialog->reject();
      } else {
        modal->hide();
      }
      return;
    }
    if (auto *fileDialog = qobject_cast<QFileDialog *>(modal)) {
      modal->setProperty("tst_driven", true);
      ++fileDialogs;
      const QStringList selected = fileDialog->selectedFiles();
      *proposedPath = selected.isEmpty() ? QString() : selected.first();
      if (targetPath.isEmpty()) {
        fileDialog->reject();
      } else {
        // selectFile() skips the file-name edit while it has keyboard
        // focus, which it does once the modal dialog is shown; type the
        // name into the edit directly so accept() sees it every time.
        const QFileInfo targetInfo(targetPath);
        fileDialog->setDirectory(targetInfo.absolutePath());
        if (auto *nameEdit = fileDialog->findChild<QLineEdit *>(
                QStringLiteral("fileNameEdit"))) {
          nameEdit->setText(targetInfo.fileName());
        } else {
          fileDialog->selectFile(targetPath);
        }
        // QFileDialog re-declares accept() protected; the QDialog view of it
        // is public and still dispatches virtually to the QFileDialog logic.
        // Run it from a posted event rather than inside this timer slot: a
        // nested exec() started from a timer activation (QFileDialog's own
        // overwrite prompt) no longer receives this timer's timeouts, so the
        // poker could neither drive that prompt nor unstick the dialog.
        modal->setProperty("tst_pending", true);
        QMetaObject::invokeMethod(
            fileDialog,
            [fileDialog]() {
              fileDialog->setProperty("tst_pending", false);
              static_cast<QDialog *>(fileDialog)->accept();
            },
            Qt::QueuedConnection);
      }
      return;
    }
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      modal->setProperty("tst_driven", true);
      *warningText = box->text();
      box->accept();
    }
  });
  poker.start();
  // click() runs the slot, and thus the nested modal loops, synchronously.
  button->click();
  poker.stop();
  if (!stuck.isEmpty()) {
    return QStringLiteral("modal dialog(s) did not close when driven: %1")
        .arg(stuck.join(QStringLiteral(", ")));
  }
  if (fileDialogs != 1) {
    return QStringLiteral("expected exactly one file dialog, saw %1")
        .arg(fileDialogs);
  }
  return {};
}

/**
 * @brief Pins the happy path of Save: the file dialog is pre-filled with
 *        "<sanitized key id>.asc" and accepting a path writes exactly the
 *        displayed armored key to that file.
 */
void tst_exportpublickeydialog::saveButtonWritesKeyToChosenFile() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), qPrintable(dir.errorString()));
  const QString armored =
      QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                     "-----END PGP PUBLIC KEY BLOCK-----\n");
  ExportPublicKeyDialog dialog(QStringLiteral("DEAD/BEEF FEEDFACE"), armored);
  const QString target = dir.filePath(QStringLiteral("exported.asc"));

  QString proposed;
  QString warning;
  const QString error = driveSaveDialog(&dialog, target, &proposed, &warning);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(QFileInfo(proposed).fileName(), QStringLiteral("DEADBEEF.asc"));
  QVERIFY2(warning.isEmpty(), qPrintable(warning));

  QFile saved(target);
  QVERIFY2(saved.exists(), "accepting the dialog must create the file");
  QVERIFY2(saved.open(QIODevice::ReadOnly | QIODevice::Text),
           qPrintable(saved.errorString()));
  QCOMPARE(QString::fromUtf8(saved.readAll()), armored);
  // The dialog wrote the chosen file and nothing else (QSaveFile leaves no
  // temporary behind on commit).
  QCOMPARE(
      QDir(dir.path())
          .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
      QStringList{QStringLiteral("exported.asc")});
}

/**
 * @brief Pins that cancelling the file dialog writes nothing and shows no
 *        warning, and that a key id with no filename-safe characters falls
 *        back to the "public_key.asc" default name. The working directory is
 *        moved into an empty temporary folder first so the dialog proposes a
 *        path there, and "nothing written" is checked where a wrong
 *        implementation would actually have written.
 */
void tst_exportpublickeydialog::saveButtonCancelWritesNothing() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), qPrintable(dir.errorString()));
  const QString previousCwd = QDir::currentPath();
  QVERIFY2(QDir::setCurrent(dir.path()), qPrintable(dir.path()));
  const auto restoreCwd =
      qScopeGuard([&previousCwd]() { QDir::setCurrent(previousCwd); });
  ExportPublicKeyDialog dialog(QStringLiteral("@#$%"),
                               QStringLiteral("payload"));

  QString proposed;
  QString warning;
  const QString error =
      driveSaveDialog(&dialog, QString(), &proposed, &warning);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(QFileInfo(proposed).fileName(), QStringLiteral("public_key.asc"));
  QVERIFY2(warning.isEmpty(), qPrintable(warning));
  QVERIFY2(!QFile::exists(proposed), qPrintable(proposed));
  QVERIFY2(
      QDir(dir.path())
          .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)
          .isEmpty(),
      "cancel must not write any file");
}

/**
 * @brief Pins the open-failure path: when the chosen file cannot be opened
 *        for writing the user gets a warning naming the path, and nothing is
 *        left behind on disk.
 */
void tst_exportpublickeydialog::saveButtonWarnsWhenFileCannotBeOpened() {
#ifdef Q_OS_WIN
  QSKIP("Directory permissions are not enforced the same way on Windows");
#endif
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), qPrintable(dir.errorString()));
  QVERIFY2(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner |
                                                 QFileDevice::ExeOwner),
           "could not make the temporary directory read-only");
  const auto restore = qScopeGuard([&dir]() {
    QFile::setPermissions(dir.path(), QFileDevice::ReadOwner |
                                          QFileDevice::WriteOwner |
                                          QFileDevice::ExeOwner);
  });
  const QString target = dir.filePath(QStringLiteral("readonly.asc"));
  {
    QFile probe(target);
    if (probe.open(QIODevice::WriteOnly)) {
      probe.close();
      QFile::remove(target);
      QSKIP("Running with privileges that ignore directory permissions");
    }
  }
  ExportPublicKeyDialog dialog(QStringLiteral("DEADBEEF"),
                               QStringLiteral("payload"));

  QString proposed;
  QString warning;
  const QString error = driveSaveDialog(&dialog, target, &proposed, &warning);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QVERIFY2(warning.contains(QStringLiteral("Could not open")),
           qPrintable(warning));
  QVERIFY2(warning.contains(target), qPrintable(warning));
  QVERIFY2(!QFile::exists(target), "a failed open must not leave a file");
}

QTEST_MAIN(tst_exportpublickeydialog)
#include "tst_exportpublickeydialog.moc"
