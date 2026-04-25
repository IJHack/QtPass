// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "importkeydialog.h"

#include "executor.h"
#include "qtpasssettings.h"
#include "ui_importkeydialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QString>

// Locale-independent: gpg's machine-readable status output via --status-fd 1.
// See doc/DETAILS in the GnuPG source for IMPORT_OK / IMPORTED grammar.
static const QRegularExpression
    IMPORT_OK_RE(QStringLiteral(R"(\[GNUPG:\] IMPORT_OK \d+ ([0-9A-Fa-f]+))"));
static const QRegularExpression
    IMPORTED_RE(QStringLiteral(R"(\[GNUPG:\] IMPORTED ([0-9A-Fa-f]+))"));
// Fallback for the human-readable (English-locale) line.
static const QRegularExpression KEY_IMPORTED_FALLBACK(
    QStringLiteral(R"(gpg: key ([0-9A-Fa-f]+):.*imported)"));

ImportKeyDialog::ImportKeyDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ImportKeyDialog) {
  ui->setupUi(this);
  ui->importButton->setEnabled(false);
}

ImportKeyDialog::~ImportKeyDialog() = default;

auto ImportKeyDialog::importedKeyId() const -> QString {
  return m_importedKeyId;
}

void ImportKeyDialog::on_fileButton_clicked() {
  const QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import GPG Key"), QString(),
      tr("ASCII-armored GPG key") + " (*.asc);;" + tr("All Files") + " (*)");

  if (fileName.isEmpty()) {
    return;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("Import Key"),
                         tr("Could not open file: %1").arg(fileName));
    return;
  }

  const QByteArray bytes = file.readAll();
  file.close();

  // Only ASCII-armored content is shown in the text edit; binary keyrings
  // would lose bytes through UTF-8 conversion. Reject anything that doesn't
  // start with the PGP armor header.
  if (!bytes.trimmed().startsWith("-----BEGIN PGP")) {
    // Message body is rich text (uses <code>/<b>); escape the path so any
    // characters in it cannot reach the HTML subset Qt renders.
    QMessageBox::warning(
        this, tr("Import Key"),
        tr("%1 does not look like an ASCII-armored GPG key. Convert it with "
           "<code>gpg --armor --export</code> first, or paste the armored "
           "block via <b>From Clipboard</b>.")
            .arg(fileName.toHtmlEscaped()));
    return;
  }

  ui->inputTextEdit->setPlainText(QString::fromUtf8(bytes));
}

void ImportKeyDialog::on_pasteButton_clicked() {
  const QClipboard *clipboard = QApplication::clipboard();
  const QString text = clipboard->text();
  if (text.isEmpty()) {
    // Don't wipe whatever the user already typed/loaded for an empty
    // clipboard.
    return;
  }
  ui->inputTextEdit->setPlainText(text);
}

void ImportKeyDialog::on_importButton_clicked() {
  const QString input = ui->inputTextEdit->toPlainText().trimmed();
  if (input.isEmpty()) {
    return;
  }

  if (importFromString(input)) {
    accept();
  }
}

void ImportKeyDialog::on_inputTextEdit_textChanged() {
  const bool hasInput = !ui->inputTextEdit->toPlainText().trimmed().isEmpty();
  ui->importButton->setEnabled(hasInput);
}

bool ImportKeyDialog::importFromString(const QString &input) {
  QString gpgExe = QtPassSettings::getGpgExecutable();
  if (gpgExe.isEmpty()) {
    gpgExe = "gpg";
  }
  QStringList args = {"--status-fd", "1", "--import", "--batch", "--yes"};

  QString stdOut;
  QString stdErr;

  int result = Executor::executeBlocking(gpgExe, args, input, &stdOut, &stdErr);

  if (result != 0) {
    showError(tr("GPG import failed:\n%1").arg(stdErr));
    return false;
  }

  QString keyId = parseGpgImportOutput(stdOut);
  if (keyId.isEmpty()) {
    keyId = parseGpgImportOutput(stdErr);
  }

  if (keyId.isEmpty()) {
    showError(tr("Could not parse imported key id from GPG output."));
    return false;
  }

  m_importedKeyId = keyId;
  showSuccess(keyId);
  return true;
}

auto ImportKeyDialog::parseGpgImportOutput(const QString &output) -> QString {
  const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  // Prefer locale-independent status output. IMPORT_OK gives a full
  // fingerprint; IMPORTED gives a (long) key id. Try both before falling
  // back to the English-only human-readable line.
  for (const QString &line : lines) {
    const QRegularExpressionMatch match = IMPORT_OK_RE.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
  }
  for (const QString &line : lines) {
    const QRegularExpressionMatch match = IMPORTED_RE.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
  }
  for (const QString &line : lines) {
    const QRegularExpressionMatch match = KEY_IMPORTED_FALLBACK.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
  }
  return QString();
}

void ImportKeyDialog::showError(const QString &message) {
  QMessageBox::warning(this, tr("Import Key"), message);
}

void ImportKeyDialog::showSuccess(const QString &keyId) {
  QMessageBox::information(this, tr("Import Key"),
                           tr("Successfully imported key: %1").arg(keyId));
}