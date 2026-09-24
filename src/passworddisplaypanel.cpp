// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include "passworddisplaypanel.h"
#include "appsettings.h"
#include "otpcodewidget.h"
#include "qpushbuttonasqrcode.h"
#include "qpushbuttonshowpassword.h"
#include "qpushbuttonwithclipboard.h"
#include "totp.h"
#include "util.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>

PasswordDisplayPanel::PasswordDisplayPanel(QGridLayout *grid,
                                           QBoxLayout *container,
                                           QWidget *widgetParent,
                                           QObject *parent)
    : QObject(parent), m_grid(grid), m_container(container),
      m_widgetParent(widgetParent) {}

void PasswordDisplayPanel::clear() {
  while (m_grid->count() > 0) {
    QLayoutItem *item = m_grid->takeAt(0);
    if (QWidget *widget = item->widget()) {
      delete widget;
    }
    delete item;
  }
  m_container->setSpacing(0);
}

auto PasswordDisplayPanel::currentOtpCode() const -> QString {
  // displayFields() renders at most one OTP row, so the first match is it.
  for (int i = 0; i < m_grid->count(); ++i) {
    QWidget *widget = m_grid->itemAt(i)->widget();
    if (widget == nullptr) {
      continue;
    }
    if (auto *otp = widget->findChild<OtpCodeWidget *>()) {
      return otp->code();
    }
  }
  return {};
}

void PasswordDisplayPanel::displayFields(const QString &password,
                                         const NamedValues &namedValues,
                                         const AppSettings &s,
                                         const QString &otpConfig) {
  // Rows are numbered as they are added rather than assuming the password
  // occupies row 0: an entry written by `pass otp insert` has no password row,
  // and starting at 1 regardless left an empty grid row above the OTP row.
  int position = 0;
  // Defence in depth: getPasswordForDisplay() already blanks a password line
  // that is an otpauth URI (`pass otp insert`); refuse one here too.
  if (!password.isEmpty() && !FileContent::isOtpUriValue(password)) {
    // The password is hidden in addField when needed.
    addField(position, QObject::tr("Password"), password, s);
    ++position;
  }
  bool otpRendered = false;
  for (const NamedValue &nv : namedValues) {
    // Keyed on the value too: a field called anything whose value is an otpauth
    // URI is still a shared secret, and isLineHidden() hides it from the text
    // browser, so suppressing only OTP/TOTP names leaked `2fa:` and friends.
    if (FileContent::isOtpFieldName(nv.name) ||
        FileContent::isOtpUriValue(nv.value)) {
      // Never render an OTP field verbatim: addField() would display the
      // shared secret and hand it to a copy button. Unconditional, so it stays
      // hidden when OTP support is off and otpConfig is empty.
      if (!otpRendered && !otpConfig.isEmpty()) {
        addOtpField(position, otpConfig, s);
        ++position;
        otpRendered = true;
      }
      continue;
    }
    addField(position, nv.name, nv.value, s);
    ++position;
  }
  // The configuration came from the entry body rather than from a field.
  if (!otpConfig.isEmpty() && !otpRendered) {
    addOtpField(position, otpConfig, s);
  }
  m_container->setSpacing(m_grid->count() == 0 ? 0 : 6);
}

void PasswordDisplayPanel::addField(int position, const QString &field,
                                    const QString &value,
                                    const AppSettings &s) {
  QString trimmedField = field.trimmed();
  QString trimmedValue = value.trimmed();

  // Scope every rule to the widget type so the transparent background does not
  // cascade into the field's standard context menu (a child QMenu), which would
  // otherwise render transparent too.
  const QString buttonStyle =
      "QPushButton { border-style: none; background: transparent; padding: 0; "
      "margin: 0; icon-size: 16px; color: inherit; }";

  auto *frame = createFieldFrame();
  QHBoxLayout *frameLayout = qobject_cast<QHBoxLayout *>(frame->layout());
  if (s.clipBoardType != Enums::CLIPBOARD_NEVER) {
    auto *fieldLabel =
        new QPushButtonWithClipboard(trimmedValue, m_widgetParent);
    connect(fieldLabel, &QPushButtonWithClipboard::clicked, this,
            &PasswordDisplayPanel::copyRequested);

    fieldLabel->setStyleSheet(buttonStyle);
    frameLayout->addWidget(fieldLabel);
  }

  if (s.useQrencode) {
    auto *qrbutton = new QPushButtonAsQRCode(trimmedValue, m_widgetParent);
    connect(qrbutton, &QPushButtonAsQRCode::clicked, this,
            &PasswordDisplayPanel::qrRequested);
    qrbutton->setStyleSheet(buttonStyle);
    frameLayout->addWidget(qrbutton);
  }

  // An explicit "open in browser" button for a safe http(s) URL, never on
  // the password field: its secret must not reach a tooltip or the browser.
  if (trimmedField != QObject::tr("Password") &&
      Util::isLaunchableWebUrl(trimmedValue)) {
    auto *urlButton = new QPushButton(m_widgetParent);
    urlButton->setIcon(QIcon::fromTheme(QStringLiteral("applications-internet"),
                                        QIcon(":/icons/open-url.svg")));
    // Escape for the tooltip only; the launched URL stays the validated value.
    // <qt> forces rich text: auto-detection would show `&amp;` literally, and
    // plain text misreads a URL containing `&lt;`. Rich text wraps, breaking
    // the URL at `/` and `?`; white-space:nowrap keeps it on one line (<nobr>
    // is not enough: Qt only makes its spaces non-breaking).
    urlButton->setToolTip(
        QStringLiteral("<qt style=\"white-space:nowrap\">%1</qt>")
            .arg(QObject::tr("Open %1 in browser")
                     .arg(trimmedValue.toHtmlEscaped())));
    urlButton->setStyleSheet(buttonStyle);
    urlButton->setCursor(Qt::PointingHandCursor);
    connect(urlButton, &QPushButton::clicked, this, [trimmedValue]() {
      // Re-validate: never hand an unvalidated string to the OS URL handler.
      if (Util::isLaunchableWebUrl(trimmedValue)) {
        QDesktopServices::openUrl(QUrl(trimmedValue));
      }
    });
    frame->layout()->addWidget(urlButton);
  }

  const QString lineStyle =
      s.useMonospace
          ? "QLineEdit, QTextBrowser { border-style: none; background: "
            "transparent; font-family: monospace; }"
          : "QLineEdit, QTextBrowser { border-style: none; background: "
            "transparent; }";

  // 26px matches the action-button visual height for consistent alignment.
  constexpr int fieldHeight = 26;
  if (s.hidePassword && trimmedField == QObject::tr("Password")) {
    auto *passwordLineEdit = new QLineEdit();
    passwordLineEdit->setObjectName(trimmedField);
    passwordLineEdit->setText(trimmedValue);
    passwordLineEdit->setReadOnly(true);
    passwordLineEdit->setStyleSheet(lineStyle);
    passwordLineEdit->setContentsMargins(0, 0, 0, 0);
    passwordLineEdit->setEchoMode(QLineEdit::Password);
    auto *showButton =
        new QPushButtonShowPassword(passwordLineEdit, m_widgetParent);
    showButton->setStyleSheet(buttonStyle);
    showButton->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(showButton);
    frame->layout()->addWidget(passwordLineEdit);
  } else {
    auto *contentTextBrowser = new QTextBrowser();
    contentTextBrowser->setOpenExternalLinks(true);
    contentTextBrowser->setOpenLinks(true);
    contentTextBrowser->setMaximumHeight(fieldHeight);
    contentTextBrowser->setMinimumHeight(fieldHeight);
    contentTextBrowser->setSizePolicy(
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    contentTextBrowser->setObjectName(trimmedField);
    {
      // Only launchable http(s) URLs become anchors (the button's predicate):
      // setOpenExternalLinks() would hand ssh://, ftp:// or user:pass@ URLs
      // straight to the OS URL handler on click.
      bool linked = false;
      const QString linkedText = Util::linkifyUrls(trimmedValue, &linked);
      if (!linked) {
        // Plain text: escaping plus setText() showed `a&b` as `a&amp;b`
        // unless the value happened to contain a '<'.
        contentTextBrowser->setPlainText(trimmedValue);
      } else {
        // Always HTML here: the text is escaped and carries anchor tags, so
        // do not leave the interpretation to setText()'s auto-detection.
        contentTextBrowser->setHtml(linkedText);
      }
    }
    contentTextBrowser->setReadOnly(true);
    contentTextBrowser->setStyleSheet(lineStyle);
    contentTextBrowser->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(contentTextBrowser);
  }

  addRow(position, new QLabel(trimmedField), frame);
}

void PasswordDisplayPanel::addRow(int position, QLabel *label, QFrame *frame) {
  m_grid->addWidget(label, position, 0);
  m_grid->addWidget(frame, position, 1);
  label->installEventFilter(this);
  frame->installEventFilter(this);
  // The value widgets swallow mouse events before the frame sees them.
  for (QWidget *child : frame->findChildren<QWidget *>()) {
    child->installEventFilter(this);
  }
}

auto PasswordDisplayPanel::eventFilter(QObject *watched, QEvent *event)
    -> bool {
  if (event->type() == QEvent::MouseButtonDblClick &&
      static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
    emit editRequested();
    return true;
  }
  return QObject::eventFilter(watched, event);
}

// The colour is baked into the stylesheet (a `palette(mid)` reference
// resolves once at polish time), so refreshPalette() re-runs this.
void PasswordDisplayPanel::applyFrameStyle(QFrame *frame) const {
  const QString borderColor =
      m_widgetParent->palette().color(QPalette::Mid).name();
  frame->setStyleSheet(QStringLiteral(".QFrame{border: 1px solid %1; "
                                      "border-radius: 5px;}")
                           .arg(borderColor));
}

void PasswordDisplayPanel::refreshPalette() {
  for (int i = 0; i < m_grid->count(); ++i) {
    if (auto *frame = qobject_cast<QFrame *>(m_grid->itemAt(i)->widget())) {
      applyFrameStyle(frame);
    }
  }
}

// Shared by addField() and addOtpField() so they cannot drift apart on
// spacing or border colour.
auto PasswordDisplayPanel::createFieldFrame() -> QFrame * {
  auto *frame = new QFrame();
  auto *frameLayout = new QHBoxLayout();
  frameLayout->setContentsMargins(5, 2, 2, 2);
  frameLayout->setSpacing(0);
  frame->setLayout(frameLayout);
  applyFrameStyle(frame);
  return frame;
}

// One label plus one frame, like every row, so grid rows match
// displayFields()' position counter. hidePassword does not apply (the code
// expires in seconds, next to a visible countdown), and there is no QR
// button: a QR of the configuration would show the shared secret.
void PasswordDisplayPanel::addOtpField(int position, const QString &otpConfig,
                                       const AppSettings &s) {
  const std::optional<Totp::Settings> settings = Totp::parse(otpConfig);
  if (!settings.has_value()) {
    // Report the problem without echoing what the user stored: the value is
    // still a would-be secret, so it gets neither a copy nor a QR button.
    AppSettings inert = s;
    inert.clipBoardType = Enums::CLIPBOARD_NEVER;
    inert.useQrencode = false;
    addField(position, QObject::tr("OTP code"),
             QObject::tr("No OTP code found in this password entry"), inert);
    return;
  }

  auto *frame = createFieldFrame();
  auto *otpWidget =
      new OtpCodeWidget(*settings, s.clipBoardType != Enums::CLIPBOARD_NEVER,
                        s.useMonospace, frame);
  connect(otpWidget, &OtpCodeWidget::copyRequested, this,
          &PasswordDisplayPanel::copyRequested);
  frame->layout()->addWidget(otpWidget);

  addRow(position, new QLabel(QObject::tr("OTP code")), frame);
}
