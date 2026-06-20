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
#include "qpushbuttonasqrcode.h"
#include "qpushbuttonshowpassword.h"
#include "qpushbuttonwithclipboard.h"
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

PasswordDisplayPanel::PasswordDisplayPanel(QGridLayout *grid,
                                           QBoxLayout *container,
                                           QWidget *widgetParent,
                                           QObject *parent)
    : QObject(parent), m_grid(grid), m_container(container),
      m_widgetParent(widgetParent) {}

void PasswordDisplayPanel::clear() {
  while (m_grid->count() > 0) {
    QLayoutItem *item = m_grid->takeAt(0);
    if (item->widget()) {
      delete item->widget();
    }
    // Layouts and spacers: QLayoutItem destructor handles cleanup.
    delete item;
  }
  m_container->setSpacing(0);
}

void PasswordDisplayPanel::displayFields(const QString &password,
                                         const NamedValues &namedValues,
                                         const AppSettings &s) {
  if (!password.isEmpty()) {
    // The password is hidden in addField when needed.
    addField(0, QObject::tr("Password"), password, s);
  }
  int position = 1;
  for (const NamedValue &nv : namedValues) {
    addField(position, nv.name, nv.value, s);
    ++position;
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
  auto *frame = new QFrame();
  QHBoxLayout *frameLayout = new QHBoxLayout();
  frameLayout->setContentsMargins(5, 2, 2, 2);
  frameLayout->setSpacing(0);
  frame->setLayout(frameLayout);
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
      linkedText.reserve(trimmedValue.size() * 3);
      int lastIndex = 0;
      static const QRegularExpression re = Util::protocolRegex();
      QRegularExpressionMatchIterator it = re.globalMatch(trimmedValue);
      while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
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

  // Derive the border colour from the palette so it adapts to light/dark
  // themes instead of a hardcoded light grey.
  const QString borderColor =
      m_widgetParent->palette().color(QPalette::Mid).name();
  frame->setStyleSheet(QStringLiteral(".QFrame{border: 1px solid %1; "
                                      "border-radius: 5px;}")
                           .arg(borderColor));

  // set into the layout
  m_grid->addWidget(new QLabel(trimmedField), position, 0);
  m_grid->addWidget(frame, position, 1);
}
