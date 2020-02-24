#include "qpushbuttonshowpassword.h"
#include <QTimer>

/**
 * @brief QPushButtonAsQRCode::QPushButtonAsQRCode
 *  basic constructor
 * @param textToCopy
 *  the text to display as qrcode
 * @param parent
 *  the parent window
 */
QPushButtonShowPassword::QPushButtonShowPassword(QLineEdit* line, QWidget *parent)
    : QPushButton(parent),
      iconEdit(QIcon::fromTheme("show", QIcon(":/icons/view.svg"))),
      iconEditPushed(
          QIcon::fromTheme("hide-new", QIcon(":/icons/hide.svg"))) {
  setIcon(iconEdit);
  connect(this, SIGNAL(clicked(bool)), this, SLOT(buttonClicked(bool)));
  this->line = line;
}

/**
 * @brief QPushButtonAsQRCode::buttonClicked handles clicked event by
 * emitting clicked(QString) with string provided to constructor
 */
void QPushButtonShowPassword::buttonClicked(bool) {
  if (this->line->echoMode() == QLineEdit::Password) {
    this->line->setEchoMode(QLineEdit::Normal);
    setIcon(iconEditPushed);
  } else {
    this->line->setEchoMode(QLineEdit::Password);
    setIcon(iconEdit);
  }
}
