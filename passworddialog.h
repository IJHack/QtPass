#ifndef PASSWORDDIALOG_H_
#define PASSWORDDIALOG_H_

#include "mainwindow.h"
#include <QDialog>

namespace Ui {
class PasswordDialog;
}

class PasswordDialog : public QDialog {
  Q_OBJECT

public:
  explicit PasswordDialog(MainWindow *parent = 0);
  ~PasswordDialog();
  void setPassword(QString);
  QString getPassword();
  void setTemplate(QString);
  void setFile(QString);
  void useTemplate(bool useTemplate);
  void templateAll(bool templateAll);

private slots:
  void on_checkBoxShow_stateChanged(int arg1);
  void on_createPasswordButton_clicked();

private:
  Ui::PasswordDialog *ui;
  MainWindow *mainWindow;
  QString passTemplate;
  QStringList fields;
  bool templating;
  bool allFields;
};

#endif // PASSWORDDIALOG_H_
