// SPDX-FileCopyrightText: YYYY Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "exportpublickeydialog.h"
#include "ui_exportpublickeydialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QTextStream>

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
}

/**
 * @brief ExportPublicKeyDialog::~ExportPublicKeyDialog basic destructor.
 */
ExportPublicKeyDialog::~ExportPublicKeyDialog() { delete ui; }

/**
 * @brief ExportPublicKeyDialog::on_copyButton_clicked copy the armored key
 *        text to the system clipboard.
 */
void ExportPublicKeyDialog::on_copyButton_clicked() {
  QApplication::clipboard()->setText(ui->plainTextEdit->toPlainText());
}

/**
 * @brief ExportPublicKeyDialog::on_saveButton_clicked prompt for a
 *        destination and write the armored key to that file.
 */
void ExportPublicKeyDialog::on_saveButton_clicked() {
  QString defaultName = m_keyId.isEmpty()
                            ? QStringLiteral("public_key.asc")
                            : QStringLiteral("%1.asc").arg(m_keyId);
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Public Key"), defaultName,
      tr("ASCII-armored key (*.asc);;All files (*)"));
  if (fileName.isEmpty()) {
    return;
  }
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Save Public Key"),
                         tr("Could not open %1 for writing.").arg(fileName));
    return;
  }
  QTextStream out(&file);
  out << ui->plainTextEdit->toPlainText();
}
