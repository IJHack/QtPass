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

static const QRegularExpression
    KEY_IMPORTED_EXACT(QStringLiteral(R"(gpg: key ([0-9A-Fa-f]+): imported)"));
static const QRegularExpression KEY_IMPORTED_GENERAL(
    QStringLiteral(R"(gpg: key ([0-9A-Fa-f]+):.*imported)"));

ImportKeyDialog::ImportKeyDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ImportKeyDialog) {
  ui->setupUi(this);

  ui->inputTextEdit->setPlaceholderText(
      tr("Paste an ASCII-armored GPG key here..."));

  ui->importButton->setEnabled(false);
}

ImportKeyDialog::~ImportKeyDialog() = default;

QString ImportKeyDialog::importedKeyFingerprint() const {
  return m_importedKeyId;
}

void ImportKeyDialog::on_fileButton_clicked() {
  const QString fileName = QFileDialog::getOpenFileName(
      this, tr("Import GPG Key"), QString(),
      tr("GPG Key Files") + " (*.asc *.key);;" + tr("All Files") + " (*)");

  if (fileName.isEmpty()) {
    return;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("Import Key"),
                         tr("Could not open file: %1").arg(fileName));
    return;
  }

  QByteArray bytes = file.readAll();
  file.close();

  ui->inputTextEdit->setPlainText(QString::fromUtf8(bytes));
}

void ImportKeyDialog::on_pasteButton_clicked() {
  QClipboard *clipboard = QApplication::clipboard();
  const QString text = clipboard->text();

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

QString ImportKeyDialog::parseGpgImportOutput(const QString &output) {
  const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    QRegularExpressionMatch match = KEY_IMPORTED_EXACT.match(line);
    if (match.hasMatch()) {
      return match.captured(1);
    }
    match = KEY_IMPORTED_GENERAL.match(line);
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