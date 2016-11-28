#ifndef PASSWORDDIALOG_H_
#define PASSWORDDIALOG_H_

#include "datahelpers.h"
#include "pass.h"
#include <QDialog>
#include <QWidget>

namespace Ui {
class PasswordDialog;
}

/*!
    \class PasswordDialog
    \brief PasswordDialog Handles the inserting and editing of passwords.

    Includes templated views.
 */
class PasswordDialog : public QDialog {
  Q_OBJECT

public:
  explicit PasswordDialog(const passwordConfiguration &passConfig, Pass &pass,
                          QWidget *parent = 0);
  ~PasswordDialog();

  /*! Sets content in the password field in the interface.
      \param password the password as a QString
      \sa getPassword
   */
  void setPassword(QString password);

  /*! Returns the password as set in the password field in the interface.
      \return password as a QString
      \sa setPassword
   */
  QString getPassword();

  /*! Sets content in the template for the interface.
      \param rawFields is the template as a QString
   */
  void setTemplate(QString);

  /*! Sets the file (name) in the interface.
      \param file name as a QString
   */
  void setFile(QString);

  void useTemplate(bool useTemplate);
  void templateAll(bool templateAll);
  void setLength(int l);
  void setPasswordCharTemplate(int t);
  void usePwgen(bool usePwgen);

private slots:
  void on_checkBoxShow_stateChanged(int arg1);
  void on_createPasswordButton_clicked();

private:
  Ui::PasswordDialog *ui;
  const passwordConfiguration &m_passConfig;
  Pass &m_pass;
  QString passTemplate;
  QStringList fields;
  bool templating;
  bool allFields;
};

#endif // PASSWORDDIALOG_H_
