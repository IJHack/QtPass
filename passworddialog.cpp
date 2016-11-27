#include "passworddialog.h"
#include "ui_passworddialog.h"
#include <QDebug>
#include <QLabel>
#include <QLineEdit>

/**
 * @brief PasswordDialog::PasswordDialog basic constructor.
 * @param parent
 */
PasswordDialog::PasswordDialog(const passwordConfiguration &passConfig,
                               Pass &pass, QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_passConfig(passConfig),
      m_pass(pass) {
  templating = false;
  allFields = false;
  ui->setupUi(this);
  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);
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
  if (arg1)
    ui->lineEditPassword->setEchoMode(QLineEdit::Normal);
  else
    ui->lineEditPassword->setEchoMode(QLineEdit::Password);
}

/**
 * @brief PasswordDialog::on_createPasswordButton_clicked generate a random
 * passwords.
 * @todo refactor when process is untangled from MainWindow class.
 */
void PasswordDialog::on_createPasswordButton_clicked() {
  ui->widget->setEnabled(false);
  QString newPass = m_pass.Generate(
      ui->spinBox_pwdLength->value(),
      m_passConfig.Characters[(passwordConfiguration::characterSet)
                                  ui->passwordTemplateSwitch->currentIndex()]);
  if (newPass.length() > 0)
    ui->lineEditPassword->setText(newPass);
  ui->widget->setEnabled(true);
}

/**
 * @brief PasswordDialog::setPassword populate the (templated) fields.
 * @param password
 */
void PasswordDialog::setPassword(QString password) {
  QStringList tokens = password.split("\n");
  ui->lineEditPassword->setText(tokens[0]);
  tokens.pop_front();
  if (templating) {
    QWidget *previous = ui->checkBoxShow;
    for (int i = 0; i < ui->formLayout->rowCount(); ++i) {
      QLayoutItem *item = ui->formLayout->itemAt(i, QFormLayout::FieldRole);
      if (item == NULL)
        continue;
      QWidget *widget = item->widget();
      for (int j = 0; j < tokens.length(); ++j) {
        QString token = tokens.at(j);
        if (token.startsWith(widget->objectName() + ':')) {
          tokens.removeAt(j);
          QString value = token.remove(0, widget->objectName().length() + 1);
          reinterpret_cast<QLineEdit *>(widget)->setText(value.trimmed());
        }
      }
      previous = widget;
    }
    if (allFields) {
      for (int j = 0; j < tokens.length(); ++j) {
        QString token = tokens.at(j);
        if (token.contains(':')) {
          int colon = token.indexOf(':');
          QString field = token.left(colon);
          QString value = token.right(token.length() - colon - 1);
          if (!passTemplate.contains(field) && value.startsWith("//"))
            continue; // colon is probably from a url
          QLineEdit *line = new QLineEdit();
          line->setObjectName(field.trimmed());
          line->setText(value.trimmed());
          ui->formLayout->addRow(new QLabel(field), line);
          setTabOrder(previous, line);
          previous = line;
          tokens.removeAt(j);
          --j; // tokens.length() also got shortened by the remove..
        }
      }
    }
  }
  ui->plainTextEdit->insertPlainText(tokens.join("\n"));
}

/**
 * @brief PasswordDialog::getPassword  join the (templated) fields to a QString
 * for writing back.
 * @return collappsed password.
 */
QString PasswordDialog::getPassword() {
  QString passFile = ui->lineEditPassword->text() + "\n";
  for (int i = 0; i < ui->formLayout->rowCount(); ++i) {
    QLayoutItem *item = ui->formLayout->itemAt(i, QFormLayout::FieldRole);
    if (item == NULL)
      continue;
    QWidget *widget = item->widget();
    QString text = reinterpret_cast<QLineEdit *>(widget)->text();
    if (text.isEmpty())
      continue;
    passFile += widget->objectName() + ": " + text + "\n";
  }
  passFile += ui->plainTextEdit->toPlainText();
  return passFile;
}

/**
 * @brief PasswordDialog::setTemplate set the template and create the fields.
 * @param rawFields
 */
void PasswordDialog::setTemplate(QString rawFields) {
  fields = rawFields.split('\n');
  QWidget *previous = ui->checkBoxShow;
  foreach (QString field, fields) {
    if (field.isEmpty())
      continue;
    QLineEdit *line = new QLineEdit();
    line->setObjectName(field);
    ui->formLayout->addRow(new QLabel(field), line);
    setTabOrder(previous, line);
    previous = line;
  }
}

/**
 * @brief PasswordDialog::setFile show which (password) file we are editing.
 * @param file
 */
void PasswordDialog::setFile(QString file) {
  this->setWindowTitle(this->windowTitle() + " " + file);
}

/**
 * @brief PasswordDialog::templateAll basic setter for use in
 * PasswordDialog::setPassword templating all tokenisable lines.
 * @param templateAll
 */
void PasswordDialog::templateAll(bool templateAll) { allFields = templateAll; }

/**
 * @brief PasswordDialog::useTemplate basic setter for use in
 * PasswordDialog::useTemplate templating.
 * @param useTemplate
 */
void PasswordDialog::useTemplate(bool useTemplate) { templating = useTemplate; }

/**
 * @brief PasswordDialog::setLength
 * PasswordDialog::setLength password length.
 * @param l
 */
void PasswordDialog::setLength(int l) { ui->spinBox_pwdLength->setValue(l); }

/**
 * @brief PasswordDialog::setPasswordCharTemplate
 * PasswordDialog::setPasswordCharTemplate chose the template style.
 * @param t
 */
void PasswordDialog::setPasswordCharTemplate(int t) {
  ui->passwordTemplateSwitch->setCurrentIndex(t);
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
