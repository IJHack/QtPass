#ifndef QPUSHBUTTONASQRCODE_H_
#define QPUSHBUTTONASQRCODE_H_

#include <QPushButton>

/*!
    \class QPushButtonAsQRCode
    \brief Stylish widget to display the field as QR Code
*/
class QWidget;
class QPushButtonAsQRCode : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonAsQRCode(const QString &textToCopy = "",
                                    QWidget *parent = nullptr);

  QString getTextToCopy() const;
  void setTextToCopy(const QString &value);

signals:
  void clicked(QString);

private slots:
  void changeIconDefault();
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
};

#endif // QPUSHBUTTONASQRCODE_H_
