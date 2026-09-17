// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "processoutputpanel.h"
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>

ProcessOutputPanel::ProcessOutputPanel(QWidget *parent)
    : QDockWidget(tr("Process Output"), parent) {
  setObjectName(QStringLiteral("processOutputDock"));
  setFeatures(QDockWidget::DockWidgetMovable |
              QDockWidget::DockWidgetFloatable);
  setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

  auto *contents = new QWidget(this);
  contents->setObjectName(QStringLiteral("processOutputWidget"));
  auto *layout = new QHBoxLayout(contents);
  layout->setObjectName(QStringLiteral("processOutputLayout"));
  layout->setContentsMargins(0, 0, 0, 0);
  m_clearButton = new QToolButton(contents);
  m_clearButton->setObjectName(QStringLiteral("clearOutputButton"));
  m_clearButton->setText(tr("Clear"));
  m_clearButton->setToolTip(tr("Clear output"));
  layout->addWidget(m_clearButton);
  m_edit = new QTextEdit(contents);
  m_edit->setObjectName(QStringLiteral("processOutputEdit"));
  m_edit->setReadOnly(true);
  m_edit->setAcceptRichText(false);
  layout->addWidget(m_edit);
  setWidget(contents);

  connect(m_clearButton, &QToolButton::clicked, this,
          &ProcessOutputPanel::clear);

  // Hysteresis: while the user is actively dragging the slider, don't
  // touch m_autoScroll on every tick — a brief overshoot at maximum
  // would silently re-arm auto-scroll without an explicit release. Only
  // commit on slider release. Wheel/keyboard scroll never sets
  // isSliderDown(), so they still update immediately.
  QScrollBar *bar = m_edit->verticalScrollBar();
  connect(bar, &QScrollBar::valueChanged, this, [this, bar]() {
    if (bar->isSliderDown())
      return;
    m_autoScroll = bar->value() >= bar->maximum();
  });
  connect(bar, &QScrollBar::sliderReleased, this,
          [this, bar]() { m_autoScroll = bar->value() >= bar->maximum(); });
}

void ProcessOutputPanel::append(const QString &output, bool isError,
                                const QString &linePrefix) {
  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (QString &line : lines) {
    // Right-trim only: remove trailing CR and whitespace, preserve leading
    // indentation
    line.remove('\r');
    while (!line.isEmpty() && line.back().isSpace()) {
      line.chop(1);
    }
    if (line.isEmpty()) {
      continue;
    }

    m_counter++;
    const QColor textColor =
        isError ? QColor(Qt::red) : m_edit->palette().color(QPalette::Text);
    // Apply the optional prefix per line so multi-line output stays
    // attributed to its command (e.g. all 3 lines of a `git push` show
    // "git push: ..." rather than only the first).
    const QString prefixed =
        linePrefix.isEmpty() ? line : linePrefix + QStringLiteral(": ") + line;
    // white-space: pre keeps the indentation the trimming above preserved;
    // rich text would otherwise collapse it.
    m_edit->append(
        QStringLiteral(
            "<span style=\"color: %1; white-space: pre;\">%2: %3</span>")
            .arg(textColor.name(), QString::number(m_counter),
                 prefixed.toHtmlEscaped()));
  }

  limitLines();

  if (m_autoScroll) {
    m_edit->verticalScrollBar()->setValue(
        m_edit->verticalScrollBar()->maximum());
  }
}

void ProcessOutputPanel::limitLines() {
  QTextDocument *doc = m_edit->document();
  const int excess = doc->blockCount() - MaxLines;
  if (excess <= 0) {
    return;
  }
  QTextCursor cursor(doc);
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, excess);
  cursor.removeSelectedText();
}

void ProcessOutputPanel::clear() {
  m_edit->clear();
  m_counter = 0;
  m_autoScroll = true;
}

auto ProcessOutputPanel::processName(Enums::PROCESS pid) -> QString {
  switch (pid) {
  case Enums::GIT_INIT:
    return QStringLiteral("git init"); // no-tr
  case Enums::GIT_ADD:
    return QStringLiteral("git add"); // no-tr
  case Enums::GIT_COMMIT:
    return QStringLiteral("git commit"); // no-tr
  case Enums::GIT_RM:
    return QStringLiteral("git rm"); // no-tr
  case Enums::GIT_PULL:
    return QStringLiteral("git pull"); // no-tr
  case Enums::GIT_PUSH:
    return QStringLiteral("git push"); // no-tr
  case Enums::GIT_MOVE:
    return QStringLiteral("git mv"); // no-tr
  case Enums::GIT_COPY:
    // ImitatePass::Copy literally invokes `git cp` (a git-extras
    // subcommand), so the label matches what's run. Stock-git users
    // without git-extras will see the underlying "'cp' is not a git
    // command" failure surfaced in the process output panel.
    return QStringLiteral("git cp"); // no-tr
  case Enums::PASS_INSERT:
    return QStringLiteral("pass insert"); // no-tr
  case Enums::PASS_REMOVE:
    return QStringLiteral("pass rm"); // no-tr
  case Enums::PASS_INIT:
    return QStringLiteral("pass init"); // no-tr
  case Enums::PASS_MOVE:
    return QStringLiteral("pass mv"); // no-tr
  case Enums::PASS_COPY:
    return QStringLiteral("pass cp"); // no-tr
  case Enums::PASS_GREP:
    return QStringLiteral("pass grep"); // no-tr
  case Enums::GPG_GENKEYS:
    return QStringLiteral("gpg --gen-key"); // no-tr
  case Enums::PASS_SHOW:
  case Enums::PROCESS_COUNT:
  case Enums::INVALID:
    break;
  }
  return {};
}

auto ProcessOutputPanel::isSensitiveProcess(Enums::PROCESS pid) -> bool {
  switch (pid) {
  case Enums::PASS_SHOW:
  case Enums::PASS_GREP:
  case Enums::PASS_INSERT:
    return true;
  case Enums::GIT_INIT:
  case Enums::GIT_ADD:
  case Enums::GIT_COMMIT:
  case Enums::GIT_RM:
  case Enums::GIT_PULL:
  case Enums::GIT_PUSH:
  case Enums::GIT_MOVE:
  case Enums::GIT_COPY:
  case Enums::PASS_REMOVE:
  case Enums::PASS_INIT:
  case Enums::PASS_MOVE:
  case Enums::PASS_COPY:
  case Enums::GPG_GENKEYS:
  case Enums::PROCESS_COUNT:
  case Enums::INVALID:
    break;
  }
  return false;
}
