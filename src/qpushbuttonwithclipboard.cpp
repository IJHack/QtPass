#include "qpushbuttonwithclipboard.h"
#include <QTimer>

/**
 * @brief QPushButtonWithClipboard::QPushButtonWithClipboard
 *  basic constructor
 * @param textToCopy
 *  the text to paste into the clipboard
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
  connect(this, &QPushButtonWithClipboard::clicked, this,
          &QPushButtonWithClipboard::buttonClicked);
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
 * @param value QString text to be copied
 */
void QPushButtonWithClipboard::setTextToCopy(const QString &value) {
  textToCopy = value;
}

/**
 * @brief QPushButtonWithClipboard::buttonClicked handles clicked event by
 * emitting clicked(QString) with string provided to constructor
 */
void QPushButtonWithClipboard::buttonClicked(QString) {
  setIcon(iconEditPushed);
  QTimer::singleShot(500, this, SLOT(changeIconDefault()));
  emit clicked(textToCopy);
}

/**
 * @brief QPushButtonWithClipboard::changeIconDefault change the icon back to
 * the default copy icon
 */
void QPushButtonWithClipboard::changeIconDefault() { this->setIcon(iconEdit); }
