// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "exportpublickeydialog.h"
#include "ui_exportpublickeydialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

/**
 * @brief ExportPublicKeyDialog::ExportPublicKeyDialog populate the dialog
 *        with the supplied armored key text.
 * @param keyId GPG key identifier shown in the header.
 * @param armoredKey ASCII-armored public key text.
 * @param parent Optional parent widget.
 */
ExportPublicKeyDialog::ExportPublicKeyDialog(const QString &keyId,
                                             const QString &armoredKey,
                                             QWidget *parent)
    : QDialog(parent), ui(new Ui::ExportPublicKeyDialog), m_keyId(keyId) {
  ui->setupUi(this);
  ui->keyIdLabel->setText(tr("Public key for %1").arg(keyId));
  ui->plainTextEdit->setFont(
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->plainTextEdit->setPlainText(armoredKey);
  m_copyButtonOriginalText = ui->copyButton->text();
}

/**
 * @brief ExportPublicKeyDialog::~ExportPublicKeyDialog basic destructor.
 */
ExportPublicKeyDialog::~ExportPublicKeyDialog() { delete ui; }

/**
 * @brief ExportPublicKeyDialog::on_copyButton_clicked copy the armored key
 *        text to the system clipboard and briefly relabel the button so the
 *        user gets visible feedback.
 */
void ExportPublicKeyDialog::on_copyButton_clicked() {
  QApplication::clipboard()->setText(ui->plainTextEdit->toPlainText());
  ui->copyButton->setText(tr("Copied!"));
  QTimer::singleShot(1500, this, [this]() {
    ui->copyButton->setText(m_copyButtonOriginalText);
  });
}

/**
 * @brief ExportPublicKeyDialog::on_saveButton_clicked prompt for a
 *        destination and write the armored key to that file.
 */
void ExportPublicKeyDialog::on_saveButton_clicked() {
  // m_keyId may hold space-separated key IDs and is settings-controlled, so
  // strip it down to the first whitespace token and keep only filename-safe
  // characters before suggesting it as the default save name.
  QString safeKeyId = m_keyId.section(QChar(' '), 0, 0);
  static const QRegularExpression unsafeChars(QStringLiteral("[^A-Za-z0-9_-]"));
  safeKeyId.remove(unsafeChars);
  QString defaultName = safeKeyId.isEmpty()
                            ? QStringLiteral("public_key.asc")
                            : QStringLiteral("%1.asc").arg(safeKeyId);
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Public Key"), defaultName,
      tr("ASCII-armored key (*.asc);;All files (*)"));
  if (fileName.isEmpty()) {
    return;
  }
  QSaveFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Save Public Key"),
                         tr("Could not open %1 for writing: %2")
                             .arg(fileName, file.errorString()));
    return;
  }
  QTextStream out(&file);
  out << ui->plainTextEdit->toPlainText();
  out.flush();
  if (out.status() != QTextStream::Ok || !file.commit()) {
    QMessageBox::warning(
        this, tr("Save Public Key"),
        tr("Could not write to %1: %2").arg(fileName, file.errorString()));
  }
}
