// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief PasswordDisplayPanel: renders the decrypted fields of an entry
 *        (password, template fields, live OTP code) into the main window's
 *        grid, with copy/QR/open buttons and secret hiding.
 */
#ifndef SRC_PASSWORDDISPLAYPANEL_H_
#define SRC_PASSWORDDISPLAYPANEL_H_

#include "filecontent.h"

#include <QObject>
#include <QString>

struct AppSettings;

class QGridLayout;
class QBoxLayout;
class QFrame;
class QLabel;
class QLayout;
class QWidget;

/**
 * @class PasswordDisplayPanel
 * @ingroup ui
 * @brief Renders decrypted password-file fields into MainWindow's grid.
 *
 * Extracted from MainWindow, where the field-rendering and grid-clearing
 * logic was spread across addToGridLayout(), clearTemplateWidgets() and
 * passShowHandler()/otpFromFileToClipboard(). It builds the per-field row
 * (optional copy / QR / open-in-browser buttons plus a value widget with
 * password echo handling) and owns the show/hide spacing of the surrounding
 * container.
 *
 * It does not own the grid or container widgets (those live in MainWindow's
 * .ui); it renders into the ones it is given. Copy and QR actions are surfaced
 * as signals rather than wired to QtPass directly, so the panel has no
 * dependency on the application object and is unit-testable on its own.
 */
class PasswordDisplayPanel : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a panel that renders into the given layouts.
   * @param grid Grid layout that receives the field rows.
   * @param container Surrounding box layout whose spacing is toggled.
   * @param widgetParent Parent widget for the created field widgets.
   * @param parent QObject parent.
   */
  PasswordDisplayPanel(QGridLayout *grid, QBoxLayout *container,
                       QWidget *widgetParent, QObject *parent = nullptr);

  /**
   * @brief Remove all field rows and collapse the container spacing.
   */
  void clear();

  /**
   * @brief Re-derive palette-dependent styling after a runtime theme change.
   *
   * Field frames bake the current QPalette::Mid colour into a stylesheet;
   * call this on QEvent::PaletteChange so an entry that is already
   * on screen picks up the new theme instead of keeping the old border.
   */
  void refreshPalette();

  /**
   * @brief Render the password and template fields of a decrypted entry.
   *
   * Fields whose name satisfies FileContent::isOtpFieldName() are never
   * rendered verbatim, because their value is a TOTP shared secret. When
   * @p otpConfig is set, a live OtpCodeWidget row is rendered in that field's
   * place instead (or appended, when the configuration came from the entry
   * body rather than from a field).
   * @param password Password value (row 0); skipped when empty.
   * @param namedValues Template/named fields to render below the password.
   * @param s AppSettings snapshot supplying display settings (clipboard,
   *        monospace, hide-password, qrencode).
   * @param otpConfig Raw OTP configuration from FileContent::getOtpUri(), or
   *        an empty string to render no OTP row at all.
   */
  void displayFields(const QString &password, const NamedValues &namedValues,
                     const AppSettings &s, const QString &otpConfig = {});

  /**
   * @brief The one-time password currently on display, if there is one.
   *
   * Lets callers copy the code the user can actually see instead of decrypting
   * the entry a second time to re-derive it.
   * @return The live code, or an empty string when no OTP row is rendered
   *         (no OTP in the entry, OTP support off, or the panel was cleared).
   */
  [[nodiscard]] auto currentOtpCode() const -> QString;

  /**
   * @brief Set the field names a template marks as secret.
   *
   * With "Hide password" on, a field with one of these names, whole and in
   * any case, is hidden like the password, whatever "Also hide other
   * secrets" says: the template's writer asked for it. Kept until set again.
   * @param names The names from TemplateIO::secretFieldsIn() and
   *        TemplateIO::readSecretFields().
   */
  void setMarkedSecrets(const QStringList &names);

  /**
   * @brief Whether a field is a secret "Hide password" hides: one a
   * template marks (setMarkedSecrets()), or, with "Also hide other
   * secrets", one whose name holds a secret word. Not the password itself,
   * nor every field under "Hide every field". False with "Hide password"
   * off.
   * @param field The field's name, trimmed.
   * @param s The display settings.
   * @return true for a secret to hide.
   */
  [[nodiscard]] auto isSecret(const QString &field, const AppSettings &s) const
      -> bool;

signals:
  /**
   * @brief Emitted when a field's copy button is clicked.
   * @param text Value to copy to the clipboard.
   */
  void copyRequested(const QString &text);

  /**
   * @brief Text was selected and copied out of a field's value (Ctrl+C, the
   * context menu, a drag, or the X11 primary selection); it is already on
   * the clipboard and only needs tracking for autoclear.
   * @param text The copied text.
   */
  void textCopied(const QString &text);
  /**
   * @brief Emitted when a field's QR button is clicked.
   * @param text Value to render as a QR code.
   */
  void qrRequested(const QString &text);
  /**
   * @brief Emitted on a double-click anywhere on a field row: the user wants
   *        to change what they are looking at.
   */
  void editRequested();

protected:
  /**
   * @brief Turn a left double-click on a field row into editRequested().
   * @param watched the row's label or value frame
   * @param event the event to inspect
   * @return true when the double-click was consumed
   */
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

private:
  /**
   * @brief Render one `name: value` row: action buttons, then the value
   *        either behind the eye button (see hides()) or in a read-only
   *        browser that links http(s) URLs, then the name label.
   *
   * The password row keeps its value verbatim (only a trailing CR is
   * dropped); every other value is trimmed.
   * @param position Grid row to fill.
   * @param field The field's name; trimmed here.
   * @param value The raw value.
   * @param s Display settings (clipboard, qrencode, monospace, hiding).
   */
  void addField(int position, const QString &field, const QString &value,
                const AppSettings &s);
  /**
   * @brief Put a row's label and value frame into the grid, listening for a
   *        double-click on either (and on the frame's children, which swallow
   *        mouse events first).
   * @param position Grid row.
   * @param label The name label for column 0.
   * @param frame The value frame for column 1.
   */
  void addRow(int position, QLabel *label, QFrame *frame);
  /**
   * @brief Render the live OTP row: an OtpCodeWidget in a field frame under
   *        an "OTP code" label. A configuration Totp::parse() rejects becomes
   *        an inert message row with no copy or QR button. The code is never
   *        hidden and never offered as a QR code.
   * @param position Grid row to fill.
   * @param otpConfig Raw OTP configuration from the entry.
   * @param s Display settings (clipboard, monospace).
   */
  void addOtpField(int position, const QString &otpConfig,
                   const AppSettings &s);
  /**
   * @brief New bordered frame with a tight horizontal layout, styled by
   *        applyFrameStyle(); shared by addField() and addOtpField() so the
   *        rows cannot drift apart.
   * @return The frame; the caller adds it to the grid.
   */
  auto createFieldFrame() -> QFrame *;
  /**
   * @brief Add the copy, QR and open-in-browser buttons a field gets, per
   * the settings.
   * @param layout The field's frame layout.
   * @param field The field's name, trimmed.
   * @param value Its value, trimmed.
   * @param s Clipboard and qrencode settings.
   */
  void addActionButtons(QLayout *layout, const QString &field,
                        const QString &value, const AppSettings &s);
  /**
   * @brief Add the password, hidden, with a button that shows it.
   * @param layout The field's frame layout.
   * @param field The field's name, trimmed.
   * @param value The password.
   * @param lineStyle The style for the value widget.
   */
  void addHiddenPassword(QLayout *layout, const QString &field,
                         const QString &value, const QString &lineStyle);
  /**
   * @brief Give a field frame its rounded border in the current
   *        QPalette::Mid colour. The colour is baked into the stylesheet, so
   *        refreshPalette() re-runs this after a theme change.
   * @param frame The frame to style.
   */
  void applyFrameStyle(QFrame *frame) const;
  /**
   * @brief Whether a field is shown behind the eye button: the password,
   * a field a template marks, every field with "Hide every field", or one
   * whose name holds a secret word with "Also hide other secrets". Only
   * with "Hide password" on.
   * @param field The field's name, trimmed.
   * @param s The display settings.
   * @return true to hide the value.
   */
  [[nodiscard]] auto hides(const QString &field, const AppSettings &s) const
      -> bool;

  /**
   * @brief Field names the current template marks as secret, compared
   *        case-insensitively in isSecret(); set by setMarkedSecrets().
   */
  QStringList m_markedSecrets;
  /** @brief The grid the rows go into; owned by MainWindow's .ui. */
  QGridLayout *m_grid;
  /**
   * @brief Layout around the grid whose spacing is 0 while empty and 6 once
   *        rows are shown; owned by MainWindow's .ui.
   */
  QBoxLayout *m_container;
  /**
   * @brief Parent for the buttons the panel creates and source of the
   *        palette used by applyFrameStyle(); not owned.
   */
  QWidget *m_widgetParent;
};

#endif // SRC_PASSWORDDISPLAYPANEL_H_
