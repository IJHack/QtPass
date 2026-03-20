#ifndef QPUSHBUTTONWITHCLIPBOARD_H_
#define QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>

/*!
    \class QPushButtonWithClipboard
    \brief Stylish widget to allow copying of password and account details
*/
class QWidget;
class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonWithClipboard(const QString &textToCopy = "",
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
  QIcon iconEditPushed;
};

#endif // QPUSHBUTTONWITHCLIPBOARD_H_
