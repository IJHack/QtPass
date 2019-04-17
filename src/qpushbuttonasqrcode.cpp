#include "qpushbuttonasqrcode.h"
#include <QTimer>

/**
 * @brief QPushButtonAsQRCode::QPushButtonAsQRCode
 *  basic constructor
 * @param textToCopy
 *  the text to display as qrcode
 * @param parent
 *  the parent window
 */
QPushButtonAsQRCode::QPushButtonAsQRCode(const QString &textToCopy,
                                                   QWidget *parent)
    : QPushButton(parent), textToCopy(textToCopy),
      iconEdit(QIcon::fromTheme("qrcode", QIcon(":/icons/qrcode.svg"))) {
  setIcon(iconEdit);
  connect(this, SIGNAL(clicked(bool)), this, SLOT(buttonClicked(bool)));
}

/**
 * @brief QPushButtonAsQRCode::getTextToCopy returns the text of
 * associated text field
 * @return QString textToCopy
 */
QString QPushButtonAsQRCode::getTextToCopy() const { return textToCopy; }

/**
 * @brief QPushButtonAsQRCode::setTextToCopy sets text from associated
 * text field
 * @param value QString text to be copied
 */
void QPushButtonAsQRCode::setTextToCopy(const QString &value) {
  textToCopy = value;
}

/**
 * @brief QPushButtonAsQRCode::buttonClicked handles clicked event by
 * emitting clicked(QString) with string provided to constructor
 */
void QPushButtonAsQRCode::buttonClicked(bool) {
  emit clicked(textToCopy);
}

/**
 * @brief QPushButtonAsQRCode::changeIconDefault change the icon back to
 * the default copy icon
 */
void QPushButtonAsQRCode::changeIconDefault() { this->setIcon(iconEdit); }
