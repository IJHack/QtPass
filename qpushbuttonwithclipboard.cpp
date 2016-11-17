#include "qpushbuttonwithclipboard.h"
#include <QLabel>
#include <QDebug>

/**
 * @brief QPushButtonWithClipboard::QPushButtonWithClipboard
 *  basic constructor
 * @param textToCopy
 *  the text to paste into the clipboard
 * @param text
 *  the text for the label to display
 * @param parent
 *  the parent window
 */
QPushButtonWithClipboard::QPushButtonWithClipboard(const QString &textToCopy, MainWindow *parent) : QPushButton(*new QIcon(QIcon::fromTheme("edit-copy" ,QIcon(":/icons/edit-copy.svg"))), "", parent)
{
    this->textToCopy = textToCopy;
    this->parent = parent;
}

QString QPushButtonWithClipboard::getTextToCopy() const
{
    return textToCopy;
}

void QPushButtonWithClipboard::setTextToCopy(const QString &value)
{
    textToCopy = value;
}
