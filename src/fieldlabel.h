// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief FieldLabel: the in-place renamable, removable name label of a
 *        `key: value` field in PasswordDialog.
 */
#ifndef FIELDLABEL_H_
#define FIELDLABEL_H_

#include <QLabel>
#include <QPointer>

class QLineEdit;

/**
 * @class FieldLabel
 * @ingroup secrets
 * @brief The name of a `key: value` field in PasswordDialog, editable in
 *        place: double-click or the context menu renames it, the context
 *        menu can also remove the field (#132).
 *
 * The label only reports what the user asked for; PasswordDialog owns the
 * line edit the field belongs to and decides whether a new name is usable.
 */
class FieldLabel : public QLabel {
  Q_OBJECT

public:
  /**
   * @brief Label for the field called @p name.
   * @param name the field's key, as shown and as written back
   * @param parent owning widget
   */
  explicit FieldLabel(const QString &name, QWidget *parent = nullptr);
  /**
   * @brief Takes an open editor with it: the editor is a sibling, not a
   * child, so it would otherwise stay behind when the field is removed
   * while its name is being edited.
   */
  ~FieldLabel() override;

public slots:
  /**
   * @brief Open the in-place editor over the label.
   */
  void startEdit();

signals:
  /**
   * @brief The user typed a different name and confirmed it.
   * @param from the name before, still shown until the receiver accepts
   * @param to the new name, trimmed, never empty, without a colon or a line
   * break
   */
  void renamed(const QString &from, const QString &to);
  /**
   * @brief The user chose "Remove field".
   */
  void removeRequested();

protected:
  /**
   * @brief A left double-click starts editing the name.
   * @param event the mouse event
   */
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  /**
   * @brief Menu with Rename field and Remove field.
   * @param event the context menu event
   */
  void contextMenuEvent(QContextMenuEvent *event) override;
  /**
   * @brief Escape in the editor cancels the edit.
   * @param watched the editor
   * @param event the event to inspect
   * @return true when the event was consumed
   */
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

private:
  /**
   * @brief Close the editor, putting the label back in its place.
   * @param commit emit renamed() for a changed, non-empty name
   */
  void finishEdit(bool commit);

  /**
   * @brief The line edit open over the label while a rename is in progress;
   *        null otherwise. A sibling in the parent's form layout rather than
   *        a child, so the destructor deletes it explicitly.
   */
  QPointer<QLineEdit> m_editor;
};

#endif // FIELDLABEL_H_
