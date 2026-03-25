// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONWITHCLIPBOARD_H_
#define SRC_QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>

/*!
    \class QPushButtonWithClipboard
    \brief Stylish widget to allow copying of password and account details
*/
class QWidget;
class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonWithClipboard(QString textToCopy = "",
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
  QIcon iconEditPushed;
};

#endif // SRC_QPUSHBUTTONWITHCLIPBOARD_H_
