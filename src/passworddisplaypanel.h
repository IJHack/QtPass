// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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
class QWidget;

/**
 * @class PasswordDisplayPanel
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

signals:
  /**
   * @brief Emitted when a field's copy button is clicked.
   * @param text Value to copy to the clipboard.
   */
  void copyRequested(const QString &text);
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
  void addField(int position, const QString &field, const QString &value,
                const AppSettings &s);
  /**
   * @brief Put a row's label and value frame into the grid, listening for a
   *        double-click on either.
   */
  void addRow(int position, QLabel *label, QFrame *frame);
  void addOtpField(int position, const QString &otpConfig,
                   const AppSettings &s);
  auto createFieldFrame() -> QFrame *;
  void applyFrameStyle(QFrame *frame) const;

  QGridLayout *m_grid;
  QBoxLayout *m_container;
  QWidget *m_widgetParent;
};

#endif // SRC_PASSWORDDISPLAYPANEL_H_
