// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "keygendialog.h"
#include "configdialog.h"
#include "pass.h"
#include "qprogressindicator.h"
#include "qtpasssettings.h"
#include "ui_keygendialog.h"
#include "util.h"
#include <QCheckBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief KeygenDialog::KeygenDialog basic constructor.
 * @param parent
 */
KeygenDialog::KeygenDialog(const QString &gpgExe, ConfigDialog *parent)
    : QDialog(parent), ui(new Ui::KeygenDialog), m_progressIndicator(nullptr) {
  ui->setupUi(this);
  dialog = parent;
  connect(ui->checkBox, &QCheckBox::toggled, this,
          &KeygenDialog::setExpertMode);

  // Restore dialog state
  QByteArray savedGeometry = QtPassSettings::getDialogGeometry("keygenDialog");
  bool hasSavedGeometry = !savedGeometry.isEmpty();
  if (hasSavedGeometry) {
    restoreGeometry(savedGeometry);
  }
  if (QtPassSettings::isDialogMaximized("keygenDialog")) {
    showMaximized();
  } else if (hasSavedGeometry) {
    move(QtPassSettings::getDialogPos("keygenDialog"));
    resize(QtPassSettings::getDialogSize("keygenDialog"));
  } else {
    // Let window manager handle positioning for first launch
  }

  ui->plainTextEdit->setPlainText(Pass::getDefaultKeyTemplate(gpgExe));
  // The batch template is only of interest in expert mode; keep it out of
  // sight otherwise so the %no-protection placeholder cannot be mistaken for
  // the effective setting once a passphrase has been typed.
  ui->plainTextEdit->setVisible(ui->checkBox->isChecked());
}

/**
 * @brief KeygenDialog::~KeygenDialog even more basic destructor.
 */
KeygenDialog::~KeygenDialog() { delete ui; }

/**
 * @brief KeygenDialog::on_passphrase1_textChanged only allow OK once both
 * passphrase fields agree.
 *
 * The passphrase is deliberately never written into the template box: that
 * widget is readable (and selectable) at all times, which would defeat the
 * masked input fields. It is spliced into the batch by applyPassphrase() when
 * the dialog is accepted.
 * @param arg1
 */
void KeygenDialog::on_passphrase1_textChanged(const QString &arg1) {
  Q_UNUSED(arg1)
  ui->buttonBox->setEnabled(ui->passphrase1->text() == ui->passphrase2->text());
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
      m_progressIndicator->startAnimation();
      m_progressIndicator->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding);
      this->layout()->addWidget(m_progressIndicator.get());
    }

    ui->frame->hide();
    ui->label->setText(
        tr("This operation can take some minutes.<br />"
           "We need to generate a lot of random bytes. It is a good idea to "
           "perform some other action (type on the keyboard, move the mouse, "
           "utilize the disks) during the prime generation; this gives the "
           "random number generator a better chance to gain enough entropy."));

    this->show();
    dialog->genKey(applyPassphrase(ui->plainTextEdit->toPlainText(),
                                   ui->passphrase1->text()),
                   this);
  } else { //  cancel, close or exc was pressed
    QDialog::done(r);
    return;
  }
}

/**
 * @brief KeygenDialog::closeEvent we are done here.
 * @param event
 */
void KeygenDialog::closeEvent(QCloseEvent *event) {
  QtPassSettings::setDialogGeometry("keygenDialog", saveGeometry());
  if (!isMaximized()) {
    QtPassSettings::setDialogPos("keygenDialog", pos());
    QtPassSettings::setDialogSize("keygenDialog", size());
  }
  QtPassSettings::setDialogMaximized("keygenDialog", isMaximized());
  event->accept();
}
