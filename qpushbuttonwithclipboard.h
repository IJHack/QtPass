#ifndef QPUSHBUTTONWITHCLIPBOARD_H_
#define QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>
#include <QWidget>

class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonWithClipboard(const QString &textToCopy = "",
                                    QWidget *parent = 0);

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
