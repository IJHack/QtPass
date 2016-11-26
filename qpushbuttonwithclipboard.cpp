#include "qpushbuttonwithclipboard.h"
#include <QDebug>
#include <QLabel>

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
QPushButtonWithClipboard::QPushButtonWithClipboard(const QString &textToCopy,
                                                   MainWindow *parent)
    : QPushButton(*new QIcon(QIcon::fromTheme("edit-copy",
                                              QIcon(":/icons/edit-copy.svg"))),
                  "", parent) {
  this->textToCopy = textToCopy;
  this->parent = parent;
}

/**
 * @brief QPushButtonWithClipboard::getTextToCopy returns the text of associated
 * text field
 * @return QString textToCopy
 */
QString QPushButtonWithClipboard::getTextToCopy() const { return textToCopy; }

/**
 * @brief QPushButtonWithClipboard::setTextToCopy sets text from associated
 * text field
 * @param QString value
 */
void QPushButtonWithClipboard::setTextToCopy(const QString &value) {
  textToCopy = value;
}

void QPushButtonWithClipboard::changeIconPushed() {
  this->setIcon(*new QIcon(
      QIcon::fromTheme("document-new", QIcon(":/icons/document-new.svg"))));
  QTimer::singleShot(500, this, SLOT(changeIconDefault()));
}

void QPushButtonWithClipboard::changeIconDefault() {
  this->setIcon(*new QIcon(
      QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.svg"))));
}
