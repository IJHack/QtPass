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

KeygenDialog::~KeygenDialog() = default;

// The passphrase never goes into the template box: that widget is always
// readable, which would defeat the masked fields. applyPassphrase() splices it
// into the batch on accept.
void KeygenDialog::on_passphrase1_textChanged(const QString &arg1) {
  Q_UNUSED(arg1)
  updateOkState();
}

auto KeygenDialog::passphraseChosen() const -> bool {
  const QString passphrase = ui->passphrase1->text();
  const bool agreed = passphrase == ui->passphrase2->text();
  return agreed && (!passphrase.isEmpty() || ui->noPassphrase->isChecked());
}

void KeygenDialog::updateOkState() {
  ui->buttonBox->setEnabled(passphraseChosen());
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

void KeygenDialog::on_passphrase2_textChanged(const QString &arg1) {
  on_passphrase1_textChanged(arg1);
}

void KeygenDialog::setExpertMode(bool checked) {
  ui->plainTextEdit->setReadOnly(!checked);
  ui->plainTextEdit->setEnabled(checked);
  ui->plainTextEdit->setVisible(checked);
}

void KeygenDialog::on_email_textChanged(const QString &arg1) {
  replace("Name-Email", arg1);
}

void KeygenDialog::on_name_textChanged(const QString &arg1) {
  replace("Name-Real", arg1);
}

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

// Matches the way gpg's read_parameter_file does: leading whitespace skipped,
// keyword ends at the first whitespace, case-insensitive.
static bool isControlStatement(const QString &line, const QString &control) {
  const QString trimmed = line.trimmed();
  if (!trimmed.startsWith(control, Qt::CaseInsensitive))
    return false;
  return trimmed.size() == control.size() ||
         trimmed.at(control.size()).isSpace();
}

// Keywords are matched as gpg matches them: an expert's "%No-Protection" left
// next to the Passphrase: line makes gpg generate an unprotected key anyway.
// The value is spliced in as a plain string, never through a regex.
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

void KeygenDialog::done(int r) {
  if (QDialog::Accepted == r) {
    // The button is disabled without a passphrase choice, but done() can be
    // reached otherwise (a default button, a script); the rule holds here.
    // No dialog: the disabled OK already says what is missing.
    if (!passphraseChosen()) {
      updateOkState();
      return;
    }
    if (ui->name->text().length() < 5) {
      QMessageBox::critical(this, tr("Invalid name"),
                            tr("Name must be at least 5 characters long."));
      return;
    }

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
  } else {
    QObject::disconnect(m_finished);
    QObject::disconnect(m_failed);
    QDialog::done(r);
    return;
  }
}

// Success accepts the dialog, so an exec() caller (the first-run wizard's
// checkSecretKeys()) sees Accepted; failure gives the form back.
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

void KeygenDialog::generationFailed(const QString &error) {
  if (m_progressIndicator) {
    m_progressIndicator->stopAnimation();
    m_progressIndicator->hide();
  }
  ui->frame->show();
  ui->widget->setEnabled(true);
  ui->checkBox->setEnabled(true);
  ui->plainTextEdit->setEnabled(true);
  // OK comes back under the same rule as before the attempt, not
  // unconditionally: the passphrase choice still has to stand.
  updateOkState();
  ui->label->setText(
      tr("Key generation failed: %1").arg(error.toHtmlEscaped()));
}
