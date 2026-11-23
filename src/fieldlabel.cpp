// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements FieldLabel; startEdit() and finishEdit() swap the label
 *        and its editor in and out of the parent's QFormLayout cell.
 */
#include "fieldlabel.h"
#include "secretlineedit.h"

#include "util.h"
#include <QContextMenuEvent>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

FieldLabel::FieldLabel(const QString &name, QWidget *parent) : QLabel(parent) {
  // A field name comes from the entry, which a shared store's other writers
  // control: as rich text, <img src="/dev/zero"> would have Qt read that
  // file without end. Plain text before the name is set.
  setTextFormat(Qt::PlainText);
  setName(name);
  setToolTip(tr("Double-click to rename this field"));
}

void FieldLabel::setName(const QString &name) {
  m_name = name;
  // Bounded: the label sizes the dialog, and every & in it is a mnemonic.
  setText(Util::shownName(name));
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
  // A SecretLineEdit: in the "every key: value line" mode the name can be the
  // sensitive part, so a copy out of the editor is marked and tracked as any
  // copy out of a field.
  auto *editor = new SecretLineEdit(m_name, parentWidget());
  connect(editor, &SecretLineEdit::copied, this, &FieldLabel::copied);
  m_editor = editor;
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
  // In the tab chain just before this field's value. Created last, the editor
  // was the last tab stop, and Tab out of it wrapped to the password, all
  // selected: the next key replaced it.
  if (QWidget *value = buddy()) {
    QWidget *before = value->previousInFocusChain();
    if (before != nullptr && before != m_editor) {
      QWidget::setTabOrder(before, m_editor);
    }
    QWidget::setTabOrder(m_editor, value);
  }
  m_editor->show();
  m_editor->selectAll();
  m_editor->setFocus();
}

auto FieldLabel::eventFilter(QObject *watched, QEvent *event) -> bool {
  if (watched != m_editor || event->type() != QEvent::KeyPress) {
    return QLabel::eventFilter(watched, event);
  }
  switch (static_cast<QKeyEvent *>(event)->key()) {
  case Qt::Key_Escape:
    finishEdit(false);
    return true;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    // Enter confirms the name only; left alone it would also reach the
    // dialog's default button and save and close the whole entry.
    finishEdit(true);
    return true;
  default:
    return QLabel::eventFilter(watched, event);
  }
}

void FieldLabel::finishEdit(bool commit) {
  if (m_editor == nullptr) {
    return;
  }
  // editingFinished fires again when the editor loses focus on hide; take
  // the pointer first so the second call finds nothing to do.
  QLineEdit *editor = m_editor;
  m_editor = nullptr;
  // A colon would start the value early when the line is written back, and
  // a line break, which QLineEdit keeps from a paste, would end the line.
  const QString name =
      SecretLineEdit::withoutLineBreaks(editor->text().remove(u':')).trimmed();
  auto *form = qobject_cast<QFormLayout *>(parentWidget()->layout());
  int row = -1;
  QFormLayout::ItemRole role{};
  if (form != nullptr) {
    form->getWidgetPosition(editor, &row, &role);
  }
  // Hidden with the focus, the editor would hand it on as Tab does, which
  // could land on the password with all of it selected. The focus goes to
  // this field's value instead, without selecting it.
  if (editor->hasFocus()) {
    if (QWidget *value = buddy()) {
      value->setFocus(Qt::OtherFocusReason);
    }
  }
  editor->hide();
  if (row >= 0) {
    form->removeWidget(editor);
    form->setWidget(row, role, this);
    show();
  }
  editor->deleteLater();
  if (commit && !name.isEmpty() && name != m_name) {
    // A copy: a receiver that accepts the name calls setName(), and later
    // receivers would read the new name as the old one.
    const QString from = m_name;
    emit renamed(from, name);
  }
}
