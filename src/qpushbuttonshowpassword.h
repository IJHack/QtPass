#ifndef QPUSHBUTTONSHOWPASSWORD_H_
#define QPUSHBUTTONSHOWPASSWORD_H_

#include <QPushButton>
#include <QLineEdit>

/*!
    \class QPushButtonAsQRCode
    \brief Stylish widget to display the field as QR Code
*/
class QWidget;
class QPushButtonShowPassword : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonShowPassword(QLineEdit *line, QWidget *parent = nullptr);

signals:
  void clicked(QString);

private slots:
  void buttonClicked(bool);

private:
  QIcon iconEdit;
  QIcon iconEditPushed;
  QLineEdit *line;
};

#endif // QPUSHBUTTONSHOWPASSWORD_H_
