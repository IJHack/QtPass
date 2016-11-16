#ifndef QLABELWITHCLIPBOARD_H_
#define QLABELWITHCLIPBOARD_H_

#include "mainwindow.h"
#include <QLabel>
#include <QWidget>

class QLabelWithClipboard : public QLabel
{
    Q_OBJECT

public:
    explicit QLabelWithClipboard(const QString &textToCopy = "", const QString &text = "", MainWindow *parent = 0);

private:
    QString textToCopy;
    MainWindow *parent;

    // QWidget interface
protected:
    void mouseDoubleClickEvent(QMouseEvent *event);
};

#endif // QLABELWITHCLIPBOARD_H_
