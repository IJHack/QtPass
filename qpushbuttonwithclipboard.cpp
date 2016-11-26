#include "qpushbuttonwithclipboard.h"
#include <QTimer>

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
                                                   QWidget *parent)
    : QPushButton(parent), textToCopy(textToCopy),
      iconEdit(QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.svg"))),
      iconEditPushed(
          QIcon::fromTheme("document-new", QIcon(":/icons/document-new.svg"))) {
  setIcon(iconEdit);
  connect(this, SIGNAL(clicked(bool)), this, SLOT(buttonClicked(bool)));
}

/**
 * @brief QPushButtonWithClipboard::getTextToCopy returns the text of
 * associated text field
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

/**
 * @brief QPushButtonWithClipboard::buttonClicked handles clicked event by
 * emitting clicked(QString) with string provided to constructor
 */
void QPushButtonWithClipboard::buttonClicked(bool) {
  setIcon(iconEditPushed);
  QTimer::singleShot(500, this, SLOT(changeIconDefault()));
  emit clicked(textToCopy);
}

void QPushButtonWithClipboard::changeIconDefault() { this->setIcon(iconEdit); }
