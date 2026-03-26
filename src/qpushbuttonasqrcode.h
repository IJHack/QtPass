// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONASQRCODE_H_
#define SRC_QPUSHBUTTONASQRCODE_H_

#include <QPushButton>

/*!
    \class QPushButtonAsQRCode
    \brief Stylish widget to display the field as QR Code
*/
class QWidget;
class QPushButtonAsQRCode : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonAsQRCode(QString textToCopy = "",
                               QWidget *parent = nullptr);

  [[nodiscard]] auto getTextToCopy() const -> QString;
  void setTextToCopy(const QString &value);

Q_SIGNALS:
  void clicked(const QString &);

private Q_SLOTS:
  void changeIconDefault();
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
};

#endif // SRC_QPUSHBUTTONASQRCODE_H_
