// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fieldlabel.h"

#include <QContextMenuEvent>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

FieldLabel::FieldLabel(const QString &name, QWidget *parent)
    : QLabel(name, parent) {
  setToolTip(tr("Double-click to rename this field"));
}

void FieldLabel::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    startEdit();
    return;
  }
  QLabel::mouseDoubleClickEvent(event);
}

void FieldLabel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  menu.addAction(tr("Rename field…"), this, &FieldLabel::startEdit);
  menu.addAction(tr("Remove field"), this, &FieldLabel::removeRequested);
  menu.exec(event->globalPos());
}

FieldLabel::~FieldLabel() { delete m_editor.data(); }

void FieldLabel::startEdit() {
  if (m_editor != nullptr) {
    m_editor->setFocus();
    return;
  }
  m_editor = new QLineEdit(text(), parentWidget());
  m_editor->setObjectName(QStringLiteral("fieldNameEditor"));
  m_editor->installEventFilter(this);
  connect(m_editor, &QLineEdit::editingFinished, this,
          [this] { finishEdit(true); });
  // Take the label's cell in the form, so the editor gets the label column
  // (which widens for it) instead of floating over the value next to it.
  auto *form = qobject_cast<QFormLayout *>(parentWidget()->layout());
  int row = -1;
  QFormLayout::ItemRole role{};
  if (form != nullptr) {
    form->getWidgetPosition(this, &row, &role);
  }
  if (row >= 0) {
    form->removeWidget(this);
    hide();
    form->setWidget(row, role, m_editor);
  } else {
    m_editor->setGeometry(geometry());
  }
  m_editor->show();
  m_editor->selectAll();
  m_editor->setFocus();
}

auto FieldLabel::eventFilter(QObject *watched, QEvent *event) -> bool {
  if (watched == m_editor && event->type() == QEvent::KeyPress &&
      static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
    finishEdit(false);
    return true;
  }
  return QLabel::eventFilter(watched, event);
}

void FieldLabel::finishEdit(bool commit) {
  if (m_editor == nullptr) {
    return;
  }
  // editingFinished fires again when the editor loses focus on hide; take
  // the pointer first so the second call finds nothing to do.
  QLineEdit *editor = m_editor;
  m_editor = nullptr;
  // A colon would start the value early when the line is written back.
  const QString name = editor->text().remove(u':').trimmed();
  auto *form = qobject_cast<QFormLayout *>(parentWidget()->layout());
  int row = -1;
  QFormLayout::ItemRole role{};
  if (form != nullptr) {
    form->getWidgetPosition(editor, &row, &role);
  }
  editor->hide();
  if (row >= 0) {
    form->removeWidget(editor);
    form->setWidget(row, role, this);
    show();
  }
  editor->deleteLater();
  if (commit && !name.isEmpty() && name != text()) {
    emit renamed(text(), name);
  }
}
