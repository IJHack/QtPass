#include "qlabelwithclipboard.h"
#include <QLabel>
#include <QDebug>

/**
 * @brief QLabelWithClipboard::QLabelWithClipboard
 *  basic constructor
 * @param textToCopy
 *  the text to paste into the clipboard
 * @param text
 *  the text for the label to display
 * @param parent
 *  the parent window
 */
QLabelWithClipboard::QLabelWithClipboard(const QString &textToCopy, const QString &text, MainWindow *parent) : QLabel(text, parent)
{
    this->textToCopy = textToCopy;
    this->parent = parent;
}
/**
 * @brief QLabelWithClipboard::mouseDoubleClickEvent
 *  on doubleclick on the label paste the given textToCopy into the clipboard
 * @param event
 *  the mouse event is irrelevant
 *
 */
void QLabelWithClipboard::mouseDoubleClickEvent(QMouseEvent *event)
{
    this->parent->copyTextToClipboard(textToCopy);
}
