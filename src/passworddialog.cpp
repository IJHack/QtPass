#include "passworddialog.h"
#include "filecontent.h"
#include "pass.h"
#include "passwordconfiguration.h"
#include "qtpasssettings.h"
#include "ui_passworddialog.h"

#include <QLabel>
#include <QLineEdit>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief PasswordDialog::PasswordDialog basic constructor.
 * @param passConfig configuration constant
 * @param parent
 */
PasswordDialog::PasswordDialog(const PasswordConfiguration &passConfig,
                               QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_passConfig(passConfig) {
  m_templating = false;
  m_allFields = false;
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
PasswordDialog::PasswordDialog(const QString &file, const bool &isNew,
                               QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_file(file),
      m_isNew(isNew) {

  if (!isNew)
    QtPassSettings::getPass()->Show(m_file);

  ui->setupUi(this);

  setWindowTitle(this->windowTitle() + " " + m_file);
  m_passConfig = QtPassSettings::getPasswordConfiguration();
  usePwgen(QtPassSettings::isUsePwgen());
  setTemplate(QtPassSettings::getPassTemplate(),
              QtPassSettings::isUseTemplate());
  templateAll(QtPassSettings::isTemplateAllFields());

  setLength(m_passConfig.length);
  setPasswordCharTemplate(m_passConfig.selected);

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
  QString newPass = QtPassSettings::getPass()->Generate_b(
      static_cast<unsigned int>(ui->spinBox_pwdLength->value()),
      m_passConfig.Characters[static_cast<PasswordConfiguration::characterSet>(
          ui->passwordTemplateSwitch->currentIndex())]);
  if (newPass.length() > 0)
    ui->lineEditPassword->setText(newPass);
  ui->widget->setEnabled(true);
}

/**
 * @brief PasswordDialog::on_accepted handle Ok click for QDialog
 */
void PasswordDialog::on_accepted() {
  QString newValue = getPassword();
  if (newValue.isEmpty())
    return;

  if (newValue.right(1) != "\n")
    newValue += "\n";

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
void PasswordDialog::setPassword(QString password) {
  FileContent fileContent = FileContent::parse(
      password, m_templating ? m_fields : QStringList(), m_allFields);
  ui->lineEditPassword->setText(fileContent.getPassword());

  QWidget *previous = ui->checkBoxShow;
  // first set templated values
  NamedValues namedValues = fileContent.getNamedValues();
  for (QLineEdit *line : templateLines) {
    line->setText(namedValues.takeValue(line->objectName()));
    previous = line;
  }
  // show remaining values (if there are)
  otherLines.clear();
  for (const NamedValue &nv : namedValues) {
    QLineEdit *line = new QLineEdit();
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
 * @return collappsed password.
 */
QString PasswordDialog::getPassword() {
  QString passFile = ui->lineEditPassword->text() + "\n";
  QList<QLineEdit *> allLines(templateLines);
  allLines.append(otherLines);
  for (QLineEdit *line : allLines) {
    QString text = line->text();
    if (text.isEmpty())
      continue;
    passFile += line->objectName() + ": " + text + "\n";
  }
  passFile += ui->plainTextEdit->toPlainText();
  return passFile;
}

/**
 * @brief PasswordDialog::setTemplate set the template and create the fields.
 * @param rawFields
 */
void PasswordDialog::setTemplate(QString rawFields, bool useTemplate) {
  m_fields = rawFields.split('\n');
  m_templating = useTemplate;
  templateLines.clear();

  if (m_templating) {
    QWidget *previous = ui->checkBoxShow;
    foreach (QString field, m_fields) {
      if (field.isEmpty())
        continue;
      QLineEdit *line = new QLineEdit();
      line->setObjectName(field);
      ui->formLayout->addRow(new QLabel(field), line);
      setTabOrder(previous, line);
      templateLines.append(line);
      previous = line;
    }
  }
}

/**
 * @brief PasswordDialog::templateAll basic setter for use in
 * PasswordDialog::setPassword templating all tokenisable lines.
 * @param templateAll
 */
void PasswordDialog::templateAll(bool templateAll) {
  m_allFields = templateAll;
}

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

void PasswordDialog::setPass(const QString &output) {
  setPassword(output);
  //    TODO(bezet): enable ui
}
