// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "passworddialog.h"
#include "filecontent.h"
#include "helpers.h"
#include "pass.h"
#include "passwordconfiguration.h"
#include "qtpasssettings.h"
#include "ui_passworddialog.h"
#include "util.h"

#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief PasswordDialog::PasswordDialog basic constructor.
 * @param passConfig configuration constant
 * @param parent
 */
PasswordDialog::PasswordDialog(PasswordConfiguration passConfig,
                               QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog),
      m_passConfig(std::move(passConfig)) {
  m_templating = false;
  m_isNew = false;

  ui->setupUi(this);
  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);

  connect(QtPassSettings::getPass(), &Pass::finishedShow, this,
          &PasswordDialog::setPass);
}

/**
 * @brief PasswordDialog::PasswordDialog complete constructor.
 * @param file
 * @param isNew
 * @param parent pointer
 */
PasswordDialog::PasswordDialog(QString file, const bool &isNew, QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_file(std::move(file)),
      m_isNew(isNew) {

  if (!isNew) {
    QtPassSettings::getPass()->Show(m_file);
  }

  ui->setupUi(this);

  setWindowTitle(this->windowTitle() + " " + m_file);
  m_passConfig = QtPassSettings::getPasswordConfiguration();
  usePwgen(QtPassSettings::isUsePwgen());
  setTemplate(QtPassSettings::getPassTemplate(),
              QtPassSettings::isUseTemplate());

  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);

  connect(QtPassSettings::getPass(), &Pass::finishedShow, this,
          &PasswordDialog::setPass);
  connect(QtPassSettings::getPass(), &Pass::processErrorExit, this,
          &PasswordDialog::close);
  connect(this, &PasswordDialog::accepted, this, &PasswordDialog::on_accepted);
  connect(this, &PasswordDialog::rejected, this, &PasswordDialog::on_rejected);
}

/**
 * @brief PasswordDialog::~PasswordDialog basic destructor.
 */
PasswordDialog::~PasswordDialog() { delete ui; }

/**
 * @brief PasswordDialog::on_checkBoxShow_stateChanged hide or show passwords.
 * @param arg1
 */
void PasswordDialog::on_checkBoxShow_stateChanged(int arg1) {
  if (arg1) {
    ui->lineEditPassword->setEchoMode(QLineEdit::Normal);
  } else {
    ui->lineEditPassword->setEchoMode(QLineEdit::Password);
  }
}

/**
 * @brief PasswordDialog::on_createPasswordButton_clicked generate a random
 * password.
 */
void PasswordDialog::on_createPasswordButton_clicked() {
  ui->widget->setEnabled(false);
  const int currentIndex = ui->passwordTemplateSwitch->currentIndex();
  if (currentIndex < 0 ||
      currentIndex >= static_cast<int>(PasswordConfiguration::CHARSETS_COUNT)) {
    ui->widget->setEnabled(true);
    return;
  }

  QString newPass = QtPassSettings::getPass()->generatePassword(
      static_cast<unsigned int>(ui->spinBox_pwdLength->value()),
      m_passConfig.Characters[static_cast<PasswordConfiguration::characterSet>(
          currentIndex)]);
  if (!newPass.isEmpty()) {
    ui->lineEditPassword->setText(newPass);
  }
  ui->widget->setEnabled(true);
}

/**
 * @brief PasswordDialog::on_accepted handle Ok click for QDialog
 */
void PasswordDialog::on_accepted() {
  QString newValue = getPassword();
  if (newValue.isEmpty()) {
    return;
  }

  if (newValue.right(1) != "\n") {
    newValue += "\n";
  }

  QtPassSettings::getPass()->Insert(m_file, newValue, !m_isNew);
}

/**
 * @brief PasswordDialog::on_rejected handle Cancel click for QDialog
 */
void PasswordDialog::on_rejected() { setPassword(QString()); }

/**
 * @brief PasswordDialog::setPassword populate the (templated) fields.
 * @param password
 */
void PasswordDialog::setPassword(const QString &password) {
  // Always parse all fields as editable so users can edit any field in the
  // password file. This fixes issue #132 where users couldn't edit
  // fields without toggling the "Show all fields templated" setting.
  FileContent fileContent = FileContent::parse(password, m_fields, true);
  ui->lineEditPassword->setText(fileContent.getPassword());

  QWidget *previous = ui->checkBoxShow;
  // first set templated values
  NamedValues namedValues = fileContent.getNamedValues();
  for (QLineEdit *line : AS_CONST(templateLines)) {
    line->setText(namedValues.takeValue(line->objectName()));
    previous = line;
  }
  // show remaining values (if there are)
  otherLines.clear();
  for (const NamedValue &nv : AS_CONST(namedValues)) {
    auto *line = new QLineEdit();
    line->setObjectName(nv.name);
    line->setText(nv.value);
    ui->formLayout->addRow(new QLabel(nv.name), line);
    setTabOrder(previous, line);
    otherLines.append(line);
    previous = line;
  }

  ui->plainTextEdit->insertPlainText(fileContent.getRemainingData());
}

/**
 * @brief PasswordDialog::getPassword  join the (templated) fields to a QString
 * for writing back.
 * @return collapsed password.
 */
auto PasswordDialog::getPassword() -> QString {
  QString passFile = ui->lineEditPassword->text() + "\n";
  QList<QLineEdit *> allLines(templateLines);
  allLines.append(otherLines);
  for (QLineEdit *line : allLines) {
    QString text = line->text();
    if (text.isEmpty()) {
      continue;
    }
    passFile += line->objectName() + ": " + text + "\n";
  }
  passFile += ui->plainTextEdit->toPlainText();
  return passFile;
}

/**
 * @brief PasswordDialog::setTemplate set the template and create the fields.
 * @param rawFields
 */
void PasswordDialog::setTemplate(const QString &rawFields, bool useTemplate) {
  m_fields = rawFields.split('\n');
  m_templating = useTemplate;
  templateLines.clear();

  if (m_templating) {
    QWidget *previous = ui->checkBoxShow;
    for (const QString &field : std::as_const(m_fields)) {
      if (field.isEmpty()) {
        continue;
      }
      auto *line = new QLineEdit();
      line->setObjectName(field);
      ui->formLayout->addRow(new QLabel(field), line);
      setTabOrder(previous, line);
      templateLines.append(line);
      previous = line;
    }
  }
}

/**
 * @brief PasswordDialog::setLength
 * PasswordDialog::setLength password length.
 * @param length
 */
void PasswordDialog::setLength(int length) {
  ui->spinBox_pwdLength->setValue(length);
}

/**
 * @brief PasswordDialog::setPasswordCharTemplate
 * PasswordDialog::setPasswordCharTemplate chose the template style.
 * @param templateIndex
 */
void PasswordDialog::setPasswordCharTemplate(int templateIndex) {
  ui->passwordTemplateSwitch->setCurrentIndex(templateIndex);
}

/**
 * @brief PasswordDialog::usePwgen
 * PasswordDialog::usePwgen don't use own password generator.
 * @param usePwgen
 */
void PasswordDialog::usePwgen(bool usePwgen) {
  ui->passwordTemplateSwitch->setDisabled(usePwgen);
  ui->label_characterset->setDisabled(usePwgen);
}

/**
 * @brief Set available templates from .templates file and apply default.
 * @param templates Hash of template name to field list.
 * @param defaultTemplate Name of default template to select.
 */
void PasswordDialog::setAvailableTemplates(
    const QHash<QString, QStringList> &templates,
    const QString &defaultTemplate) {
  QStringList templateNames = templates.keys();
  if (templateNames.isEmpty()) {
    return;
  }
  QString selected = defaultTemplate;
  if (!templateNames.contains(selected)) {
    selected = templateNames.first();
  }
  auto it = templates.constFind(selected);
  if (it != templates.constEnd()) {
    QString fields = it.value().join("\n");
    setTemplate(fields, true);
  }
}

/**
 * @brief Sets the password from pass show output.
 * @param output Output from pass show command
 */
void PasswordDialog::setPass(const QString &output) {
  setPassword(output);
  // UI is enabled by default when password is set - no additional action needed
}