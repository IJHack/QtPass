// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "keygendialog.h"
#include "configdialog.h"
#include "pass.h"
#include "qprogressindicator.h"
#include "qtpasssettings.h"
#include "ui_keygendialog.h"
#include "util.h"
#include "windowstatestore.h"
#include <QCheckBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>

#include "qtpasslogging.h"

/**
 * @brief KeygenDialog::KeygenDialog basic constructor.
 * @param parent
 */
KeygenDialog::KeygenDialog(const QString &gpgExe, Pass *pass, QWidget *parent)
    : QDialog(parent), ui(new Ui::KeygenDialog), m_pass(pass),
      m_progressIndicator(nullptr) {
  ui->setupUi(this);
  connect(ui->checkBox, &QCheckBox::toggled, this,
          &KeygenDialog::setExpertMode);
  connect(ui->noPassphrase, &QCheckBox::toggled, this,
          &KeygenDialog::setNoPassphrase);
  updateOkState();

  WindowStateStore::attach(*this, QStringLiteral("keygenDialog"));

  ui->plainTextEdit->setPlainText(Pass::getDefaultKeyTemplate(gpgExe));
  // The batch template is only of interest in expert mode; keep it out of
  // sight otherwise so the %no-protection placeholder cannot be mistaken for
  // the effective setting once a passphrase has been typed.
  ui->plainTextEdit->setVisible(ui->checkBox->isChecked());
}

/**
 * @brief KeygenDialog::~KeygenDialog even more basic destructor.
 */
KeygenDialog::~KeygenDialog() = default;

/**
 * @brief KeygenDialog::on_passphrase1_textChanged only allow OK once both
 * passphrase fields agree and one was given (or waived).
 *
 * The passphrase is deliberately never written into the template box: that
 * widget is readable (and selectable) at all times, which would defeat the
 * masked input fields. It is spliced into the batch by applyPassphrase() when
 * the dialog is accepted.
 * @param arg1
 */
void KeygenDialog::on_passphrase1_textChanged(const QString &arg1) {
  Q_UNUSED(arg1)
  updateOkState();
}

void KeygenDialog::updateOkState() {
  const QString passphrase = ui->passphrase1->text();
  const bool agreed = passphrase == ui->passphrase2->text();
  const bool chosen = !passphrase.isEmpty() || ui->noPassphrase->isChecked();
  ui->buttonBox->setEnabled(agreed && chosen);
}

void KeygenDialog::setNoPassphrase(bool checked) {
  if (checked) {
    ui->passphrase1->clear();
    ui->passphrase2->clear();
  }
  ui->passphrase1->setEnabled(!checked);
  ui->passphrase2->setEnabled(!checked);
  updateOkState();
}

/**
 * @brief KeygenDialog::on_passphrase2_textChanged wrapper for
 * KeygenDialog::on_passphrase1_textChanged
 * @param arg1
 */
void KeygenDialog::on_passphrase2_textChanged(const QString &arg1) {
  on_passphrase1_textChanged(arg1);
}

/**
 * @brief KeygenDialog::setExpertMode expert mode enabled / disabled.
 * @param checked
 */
void KeygenDialog::setExpertMode(bool checked) {
  ui->plainTextEdit->setReadOnly(!checked);
  ui->plainTextEdit->setEnabled(checked);
  ui->plainTextEdit->setVisible(checked);
}

/**
 * @brief KeygenDialog::on_email_textChanged update the email in keypair
 * generation template.
 * @param arg1
 */
void KeygenDialog::on_email_textChanged(const QString &arg1) {
  replace("Name-Email", arg1);
}

/**
 * @brief KeygenDialog::on_name_textChanged update the name in keypair
 * generation template.
 * @param arg1
 */
void KeygenDialog::on_name_textChanged(const QString &arg1) {
  replace("Name-Real", arg1);
}

/**
 * @brief KeygenDialog::replace do some regex magic for replacing Name-Real and
 * Name-Email in the keypair generation template.
 * @param key
 * @param value
 */
void KeygenDialog::replace(const QString &key, const QString &value) {
  QStringList clear;
  QString expert = ui->plainTextEdit->toPlainText();
  const QStringList lines =
      expert.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
  for (QString line : lines) {
    line.replace(QRegularExpression(key + ":.*"), key + ": " + value);
    clear.append(line);
  }
  ui->plainTextEdit->setPlainText(clear.join("\n"));
}

/**
 * @brief isControlStatement match a batch line against a gpg control
 * statement the way gpg's read_parameter_file does: leading whitespace is
 * skipped, the keyword ends at the first whitespace (anything after it is
 * ignored) and the comparison is case-insensitive.
 * @param line A line of the batch template.
 * @param control The control statement, including the leading '%'.
 * @return true when gpg would honour the line as that control statement.
 */
static bool isControlStatement(const QString &line, const QString &control) {
  const QString trimmed = line.trimmed();
  if (!trimmed.startsWith(control, Qt::CaseInsensitive))
    return false;
  return trimmed.size() == control.size() ||
         trimmed.at(control.size()).isSpace();
}

/**
 * @brief KeygenDialog::applyPassphrase splice the passphrase into a GPG batch
 * template just before it is handed to gpg.
 *
 * A non-empty passphrase replaces the %no-protection control statement (or an
 * existing Passphrase: parameter) with a Passphrase: line, inserted before
 * %commit if the template has neither. An empty passphrase turns any
 * Passphrase: line back into %no-protection. The value is spliced in as a
 * plain string, never through a regex replacement.
 *
 * Keywords are matched as gpg matches them (case-insensitively, control
 * statements up to the first whitespace, parameter names up to the colon), so
 * every variant gpg would honour is caught: an expert's "%No-Protection" must
 * not survive next to the Passphrase: line, because gpg then generates an
 * unprotected key regardless of the passphrase.
 * @param batch The template as shown in the (expert) editor.
 * @param passphrase The passphrase, or an empty string for an unprotected key.
 * @return The batch to feed to gpg.
 */
QString KeygenDialog::applyPassphrase(const QString &batch,
                                      const QString &passphrase) {
  static const QString noProtection = QStringLiteral("%no-protection");
  static const QString commit = QStringLiteral("%commit");
  static const QString passphraseKey = QStringLiteral("Passphrase:");
  const QString protection =
      passphrase.isEmpty() ? noProtection : passphraseKey + " " + passphrase;

  QStringList clear;
  bool spliced = false;
  const QStringList lines =
      batch.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    if (isControlStatement(line, noProtection) ||
        line.trimmed().startsWith(passphraseKey, Qt::CaseInsensitive)) {
      if (!spliced) {
        clear.append(protection);
        spliced = true;
      }
      continue;
    }
    clear.append(line);
  }

  if (!spliced && !passphrase.isEmpty()) {
    // Parameters must precede %commit; append when the template has none.
    const auto commitLine =
        std::find_if(clear.cbegin(), clear.cend(), [](const QString &line) {
          return isControlStatement(line, commit);
        });
    clear.insert(commitLine - clear.cbegin(), protection);
  }
  return clear.join("\n");
}

/**
 * @brief KeygenDialog::done we are going to create a key pair and show the
 * QProgressIndicator and some text since the generation will take some time.
 * @param r
 */
void KeygenDialog::done(int r) {
  if (QDialog::Accepted == r) { //  ok was pressed
                                // check name
    if (ui->name->text().length() < 5) {
      QMessageBox::critical(this, tr("Invalid name"),
                            tr("Name must be at least 5 characters long."));
      return;
    }

    // check email
    static const QRegularExpression mailre(
        QRegularExpression::anchoredPattern(
            R"(\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b)"),
        QRegularExpression::CaseInsensitiveOption);
    if (!mailre.match(ui->email->text()).hasMatch()) {
      QMessageBox::critical(
          this, tr("Invalid email"),
          tr("The email address you typed is not a valid email address."));
      return;
    }

    ui->widget->setEnabled(false);
    ui->buttonBox->setEnabled(false);
    ui->checkBox->setEnabled(false);
    ui->plainTextEdit->setEnabled(false);

    if (!m_progressIndicator) {
      m_progressIndicator = std::make_unique<QProgressIndicator>();
      m_progressIndicator->setParent(this);
      m_progressIndicator->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding);
      this->layout()->addWidget(m_progressIndicator.get());
    }
    // A retry after generationFailed() reuses the indicator it stopped and hid.
    m_progressIndicator->show();
    m_progressIndicator->startAnimation();

    ui->frame->hide();
    ui->label->setText(
        tr("This operation can take some minutes.<br />"
           "We need to generate a lot of random bytes. It is a good idea to "
           "perform some other action (type on the keyboard, move the mouse, "
           "utilize the disks) during the prime generation; this gives the "
           "random number generator a better chance to gain enough entropy."));

    this->show();
    startGeneration(applyPassphrase(ui->plainTextEdit->toPlainText(),
                                    ui->passphrase1->text()));
  } else { //  cancel, close or exc was pressed
    QObject::disconnect(m_finished);
    QObject::disconnect(m_failed);
    QDialog::done(r);
    return;
  }
}

/**
 * @brief Hand the batch to the backend and wait for its verdict.
 *
 * Used to travel KeygenDialog -> ConfigDialog -> MainWindow -> QtPass -> Pass
 * with a QPointer in MainWindow relaying the result back; the dialog now
 * listens to Pass itself. Success accepts the dialog, so a caller running it
 * with exec() (the first-run wizard's checkSecretKeys()) sees Accepted;
 * failure re-enables the form so the user can fix the input or cancel.
 * @param batch The gpg --gen-key batch text, passphrase already spliced in.
 */
void KeygenDialog::startGeneration(const QString &batch) {
  if (m_pass == nullptr) {
    generationFailed(tr("No password store backend available"));
    return;
  }
  m_finished = connect(m_pass, &Pass::finishedGenerateGPGKeys, this,
                       [this](const QString &, const QString &) {
                         QObject::disconnect(m_finished);
                         QObject::disconnect(m_failed);
                         QDialog::done(QDialog::Accepted);
                       });
  // Not processErrorExit: that fires for any command that fails while the
  // key is being generated (a queued git push, say) and would take the
  // dialog down while gpg is still working.
  m_failed = connect(m_pass, &Pass::generateGPGKeysFailed, this,
                     [this](const QString &error) {
                       QObject::disconnect(m_finished);
                       QObject::disconnect(m_failed);
                       generationFailed(error);
                     });
  m_pass->GenerateGPGKeys(batch);
}

/**
 * @brief Show why generation did not happen and give the form back.
 * @param error What the backend reported.
 */
void KeygenDialog::generationFailed(const QString &error) {
  if (m_progressIndicator) {
    m_progressIndicator->stopAnimation();
    m_progressIndicator->hide();
  }
  ui->frame->show();
  ui->widget->setEnabled(true);
  ui->buttonBox->setEnabled(true);
  ui->checkBox->setEnabled(true);
  ui->plainTextEdit->setEnabled(true);
  ui->label->setText(
      tr("Key generation failed: %1").arg(error.toHtmlEscaped()));
}
