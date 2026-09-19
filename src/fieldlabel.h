#ifndef FIELDLABEL_H_
#define FIELDLABEL_H_

#include <QLabel>
#include <QPointer>

class QLineEdit;

/**
 * @class FieldLabel
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
  explicit FieldLabel(const QString &name, QWidget *parent = nullptr);

public slots:
  /**
   * @brief Open the in-place editor over the label.
   */
  void startEdit();

signals:
  /**
   * @brief The user typed a different name and confirmed it.
   * @param from the name before, still shown until the receiver accepts
   * @param to the new name, trimmed, never empty, without a colon
   */
  void renamed(const QString &from, const QString &to);
  /**
   * @brief The user chose "Remove field".
   */
  void removeRequested();

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

private:
  void finishEdit(bool commit);

  QPointer<QLineEdit> m_editor;
};

#endif // FIELDLABEL_H_
