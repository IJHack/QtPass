// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class PasswordDisplayPanel
 * @brief Password-field rendering implementation.
 *
 * @see passworddisplaypanel.h
 */

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
#include <QPalette>
#include <QPushButton>
#include <QRegularExpressionMatchIterator>
#include <QTextBrowser>
#include <QUrl>

static const QRegularExpression kProtocolRegex = Util::protocolRegex();

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

void PasswordDisplayPanel::displayFields(const QString &password,
                                         const NamedValues &namedValues,
                                         const AppSettings &s,
                                         const QString &otpConfig) {
  if (!password.isEmpty()) {
    // The password is hidden in addField when needed.
    addField(0, QObject::tr("Password"), password, s);
  }
  int position = 1;
  bool otpRendered = false;
  for (const NamedValue &nv : namedValues) {
    if (FileContent::isOtpFieldName(nv.name)) {
      // Never render an OTP field verbatim: its value is the shared secret,
      // and addField() would both display it and hand it to a copy button.
      // The skip is unconditional, so the secret stays hidden even when OTP
      // support is switched off and otpConfig is empty.
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

void PasswordDisplayPanel::appendField(const QString &field,
                                       const QString &value,
                                       const AppSettings &s) {
  // Each row is two grid items (label + value frame), so the next free row is
  // count() / 2 — the same sequential scheme displayFields() uses.
  addField(m_grid->count() / 2, field, value, s);
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

  // Combine the Copy button and the line edit in one widget
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

  // Show an explicit "open in browser" button when the value is a safe
  // http(s) URL. The inline clickable link still works for URLs embedded in
  // prose; this button is the discoverable affordance for url fields.
  // Never on the password field: its value is a secret and must not be
  // surfaced in a tooltip or handed to the browser.
  if (trimmedField != QObject::tr("Password") &&
      Util::isLaunchableWebUrl(trimmedValue)) {
    auto *urlButton = new QPushButton(m_widgetParent);
    urlButton->setIcon(QIcon::fromTheme(QStringLiteral("applications-internet"),
                                        QIcon(":/icons/open-url.svg")));
    // Escape only for tooltip rendering (rich-text safe display). The launched
    // URL must remain the original validated value; HTML escaping would change
    // it.
    urlButton->setToolTip(
        QObject::tr("Open %1 in browser").arg(trimmedValue.toHtmlEscaped()));
    urlButton->setStyleSheet(buttonStyle);
    urlButton->setCursor(Qt::PointingHandCursor);
    connect(urlButton, &QPushButton::clicked, this, [trimmedValue]() {
      // Re-validate before launching (defence in depth: the value is
      // immutable here, but never hand an unvalidated string to the OS
      // URL handler).
      if (Util::isLaunchableWebUrl(trimmedValue)) {
        QDesktopServices::openUrl(QUrl(trimmedValue));
      }
    });
    frame->layout()->addWidget(urlButton);
  }

  // set the echo mode to password, if the field is "password"
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
      QString linkedText;
      QList<QRegularExpressionMatch> urlMatches;
      qsizetype totalUrlLength = 0;
      {
        QRegularExpressionMatchIterator it =
            kProtocolRegex.globalMatch(trimmedValue);
        while (it.hasNext()) {
          QRegularExpressionMatch match = it.next();
          totalUrlLength += match.capturedLength(0);
          urlMatches.append(match);
        }
      }
      constexpr qsizetype anchorTagOverhead = sizeof("<a href=\"\"></a>") - 1;
      linkedText.reserve(trimmedValue.size() + totalUrlLength +
                         urlMatches.size() * anchorTagOverhead);
      int lastIndex = 0;
      for (const QRegularExpressionMatch &match : std::as_const(urlMatches)) {
        const int start = match.capturedStart(0);
        const int end = match.capturedEnd(0);
        linkedText +=
            trimmedValue.mid(lastIndex, start - lastIndex).toHtmlEscaped();
        const QString escapedUrl = match.captured(0).toHtmlEscaped();
        linkedText += QStringLiteral("<a href=\"%1\">%1</a>").arg(escapedUrl);
        lastIndex = end;
      }
      linkedText += trimmedValue.mid(lastIndex).toHtmlEscaped();
      contentTextBrowser->setText(linkedText);
    }
    contentTextBrowser->setReadOnly(true);
    contentTextBrowser->setStyleSheet(lineStyle);
    contentTextBrowser->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(contentTextBrowser);
  }

  // set into the layout
  m_grid->addWidget(new QLabel(trimmedField), position, 0);
  m_grid->addWidget(frame, position, 1);
}

/**
 * @brief Build the bordered container every field row's value side lives in.
 *
 * Shared by addField() and addOtpField() so the two cannot drift apart on
 * spacing or border colour.
 * @return An empty QFrame carrying a QHBoxLayout.
 */
auto PasswordDisplayPanel::createFieldFrame() -> QFrame * {
  auto *frame = new QFrame();
  auto *frameLayout = new QHBoxLayout();
  frameLayout->setContentsMargins(5, 2, 2, 2);
  frameLayout->setSpacing(0);
  frame->setLayout(frameLayout);

  // Derive the border colour from the palette so it adapts to light/dark
  // themes instead of a hardcoded light grey.
  const QString borderColor =
      m_widgetParent->palette().color(QPalette::Mid).name();
  frame->setStyleSheet(QStringLiteral(".QFrame{border: 1px solid %1; "
                                      "border-radius: 5px;}")
                           .arg(borderColor));
  return frame;
}

/**
 * @brief Render the live one-time password row.
 *
 * Exactly two grid items are added (label plus frame), like every other row,
 * so appendField()'s count()/2 row arithmetic stays valid. The code, its copy
 * button and the countdown all live inside the frame.
 *
 * AppSettings::hidePassword deliberately does not apply: it is keyed on the
 * password field and exists to protect a long-lived secret, whereas hiding a
 * code that expires in seconds behind a reveal button next to a visible
 * countdown would only get in the way. No QR button is offered either, since a
 * QR code of the configuration would put the shared secret on screen.
 * @param position Grid row to render into.
 * @param otpConfig Raw OTP configuration, as stored in the entry.
 * @param s AppSettings snapshot supplying display settings.
 */
void PasswordDisplayPanel::addOtpField(int position, const QString &otpConfig,
                                       const AppSettings &s) {
  const std::optional<Totp::Settings> settings = Totp::parse(otpConfig);
  if (!settings.has_value()) {
    // Report the problem without echoing what the user stored: the value is
    // still a would-be secret, so it gets neither a copy nor a QR button.
    AppSettings inert = s;
    inert.clipBoardType = Enums::CLIPBOARD_NEVER;
    inert.useQrencode = false;
    addField(position, QObject::tr("OTP Code"),
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

  m_grid->addWidget(new QLabel(QObject::tr("OTP Code")), position, 0);
  m_grid->addWidget(frame, position, 1);
}
