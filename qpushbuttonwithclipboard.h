#ifndef QPUSHBUTTONWITHCLIPBOARD_H_
#define QPUSHBUTTONWITHCLIPBOARD_H_

#include "mainwindow.h"
#include <QPushButton>
#include <QWidget>

class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonWithClipboard(const QString &textToCopy = "",
                                    MainWindow *parent = 0);

  QString getTextToCopy() const;
  void setTextToCopy(const QString &value);
  void changeIconPushed();

private slots:
  void changeIconDefault();

private:
  QString textToCopy;
  MainWindow *parent;
};

#endif // QPUSHBUTTONWITHCLIPBOARD_H_
