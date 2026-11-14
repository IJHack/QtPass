// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Copy-to-clipboard icon button that carries the text of the field it
 * sits next to.
 */
#ifndef SRC_QPUSHBUTTONWITHCLIPBOARD_H_
#define SRC_QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>

/**
 * @class QPushButtonWithClipboard
 * @ingroup secrets
 * @brief QPushButton that stores a text payload and emits it when activated.
 *
 * The button does not touch the clipboard itself. On activation it briefly
 * swaps to a "done" icon (reverted after 500 ms) and re-emits
 * `clicked(const QString&)` with the stored text; the receiver (normally
 * `ClipboardManager` via `PasswordDisplayPanel`) performs the copy and the
 * autoclear bookkeeping.
 */
class QWidget;
class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  /**
   * @brief Construct a QPushButtonWithClipboard with an optional initial text.
   * @param textToCopy Initial text to store and emit on activation.
   * @param parent Optional parent widget.
   */
  explicit QPushButtonWithClipboard(QString textToCopy = "",
                                    QWidget *parent = nullptr);

  /**
   * @brief Return the stored text used for copying.
   * @return The current stored text.
   */
  [[nodiscard]] auto getTextToCopy() const -> QString;
  /**
   * @brief Update the stored text used for copying.
   * @param value The new text to store.
   */
  void setTextToCopy(const QString &value);

signals:
  /**
   * @brief Emitted when the button is activated with the stored text.
   * @param text The stored text associated with this button.
   */
  void clicked(const QString &text);

private slots:
  /**
   * @brief Restores the default copy icon; fired by a single-shot timer
   * 500 ms after a click.
   */
  void changeIconDefault();
  /**
   * @brief Connected to `QPushButton::clicked(bool)`; shows the pushed icon,
   * arms the revert timer and re-emits `clicked` with the stored text.
   * @param checked Unused; the button is not checkable.
   */
  void buttonClicked(bool checked);

private:
  /** @brief Text handed to receivers on click; set by the owning panel. */
  QString textToCopy;
  /** @brief `edit-copy` icon shown at rest. */
  QIcon iconEdit;
  /** @brief `document-new` icon flashed for 500 ms after a click. */
  QIcon iconEditPushed;
};

#endif // SRC_QPUSHBUTTONWITHCLIPBOARD_H_
