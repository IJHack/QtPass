// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASSWORDDIALOG_H_
#define SRC_PASSWORDDIALOG_H_

#include "appsettings.h"

#include <QDialog>
#include <QPointer>
#include <QScopedPointer>

namespace Ui {
class PasswordDialog;
}

class Pass;
class FieldLabel;
class QAction;
class QLineEdit;
class QShortcut;
class QWidget;

/**
 * @class PasswordDialog
 * @brief Dialog for inserting and editing passwords, optionally using
 * templates.
 *
 * Manages UI for entering, generating, and templating password-related fields.
 */
class PasswordDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct a PasswordDialog for entering a new password.
   * @param passConfig Password generation configuration.
   * @param parent Optional parent widget.
   */
  explicit PasswordDialog(PasswordConfiguration passConfig,
                          QWidget *parent = nullptr);
  /**
   * @brief Construct a PasswordDialog for editing an existing password file.
   * @param pass Backend used to show, insert, and generate passwords.
   * @param s Application settings snapshot (password config, template, pwgen).
   * @param file Path to the password file being edited.
   * @param isNew true if creating a new entry, false if editing existing.
   * @param parent Optional parent widget.
   */
  PasswordDialog(Pass *pass, const AppSettings &s, QString file,
                 const bool &isNew, QWidget *parent = nullptr);
  ~PasswordDialog() override;

  /**
   * @brief OK: for a new entry, resolve and check the name first and stay
   * open when it is not usable; then close as accepted (on_accepted() does
   * the insert).
   */
  void accept() override;

  /**
   * @brief Let a new entry be named inside the dialog: a folder picker over
   * the store's folders and a name field, instead of a separate prompt.
   *
   * The dialog validates as the user types (empty, escaping the store,
   * already taken) and keeps OK off until the name is good; accept() then
   * creates any subfolder the name needs and inserts the entry at
   * entryPath(). Only meaningful for a dialog constructed with isNew.
   * @param storeRoot Absolute path of the password store.
   * @param folders Folders relative to the store, "" for the root; shown in
   *        the picker in this order.
   * @param currentFolder The folder to preselect (relative, may be "").
   */
  void setNewEntryLocation(const QString &storeRoot, const QStringList &folders,
                           const QString &currentFolder);

  /**
   * @brief The entry the dialog writes to, relative to the store and
   * without the `.gpg` suffix (for example `work/vpn`).
   * @return The path; for an existing entry the one given at construction.
   */
  auto entryPath() const -> QString { return m_file; }

  /**
   * @brief Populate the dialog's password field with the given text.
   * @param password Password text to display.
   * @sa getPassword
   */
  void setPassword(const QString &password);

  /**
   * @brief Retrieve the current text from the dialog's password field.
   * @return Current password as a QString.
   * @sa setPassword
   */
  auto getPassword() -> QString;

  /**
   * @brief Set the template fields and whether templating is enabled.
   * @param rawFields Template text containing field definitions.
   * @param useTemplate true to enable the template, false to disable.
   */
  void setTemplate(const QString &rawFields, bool useTemplate);

  /**
   * @brief Whether every `key: value` line becomes a field of its own.
   *
   * Mirrors the "Template all fields" setting. Off, only the template's
   * fields get widgets and every other line stays in the free-text body,
   * where the user can edit key and value alike.
   * @param templateAll true to split every tokenisable line into a field.
   */
  void templateAll(bool templateAll);

  /**
   * @brief Set the desired password length shown in the dialog.
   * @param length Desired password length.
   */
  void setLength(int length);

  /**
   * @brief Set the password character template index.
   * @param templateIndex Index identifying the character template to use.
   */
  void setPasswordCharTemplate(int templateIndex);

  /**
   * @brief Enable or disable pwgen-style password generation mode.
   * @param usePwgen true to enable pwgen mode, false to disable.
   */
  void usePwgen(bool usePwgen);

  /**
   * @brief Set available templates from file and select default.
   * @param templates Hash of template name to field list.
   * @param defaultTemplate Name of default template.
   */
  void setAvailableTemplates(const QHash<QString, QStringList> &templates,
                             const QString &defaultTemplate);

public slots:
  /**
   * @brief Cycle to next template (Ctrl+T).
   */
  void cycleTemplate();

  /**
   * @brief Populate the dialog's password field from pass show output.
   * @param output Output from the pass show command.
   */
  void setPass(const QString &output, const QString &file = QString());

private slots:
  /**
   * @brief Show or mask the password field.
   *
   * Connected explicitly to QCheckBox::toggled in both constructors; the box
   * is two-state, so the deprecated stateChanged(int) is not needed.
   * @param show true to display the password in clear text.
   */
  void setPasswordVisible(bool show);
  void on_createPasswordButton_clicked();
  void on_accepted();
  void on_rejected();

  /**
   * @brief Handle a process error while waiting for an entry's decrypt.
   *
   * When the asynchronous Show() fails, keep the dialog open and surface the
   * reason in statusLabel instead of closing silently: Ok stays disabled and
   * the editor keeps its lock so the user can only Cancel/withdraw.
   * @param exitCode Exit code of the failed process (unused).
   * @param err Error output to present to the user.
   */
  void onShowError(int exitCode, const QString &err);

private:
  /**
   * @brief Locate the field that holds the one-time password configuration.
   * @return The matching QLineEdit, or nullptr when the entry has no OTP field.
   */
  [[nodiscard]] auto otpLineEdit() const -> QLineEdit *;
  /**
   * @brief Enabled or grey out every editable field of the dialog.
   *
   * Used while an existing entry's decrypted content is being fetched, so an
   * early Ok cannot report success while writing nothing and a late setPass()
   * cannot clobber edits made on the still-empty fields.
   * @param enabled true to restore the editor, false to lock it.
   */
  void setEditorEnabled(bool enabled);
  /**
   * @brief Connect validation and normalisation to the OTP field, if present.
   *
   * Called whenever the field widgets are rebuilt, since both setTemplate()
   * and setPassword() recreate them.
   */
  void hookOtpField();
  /**
   * @brief Flag an OTP value that is neither a URI nor valid base32.
   */
  void validateOtpField();
  /**
   * @brief Remember that the user typed in the OTP field, so a bare secret
   * they entered is canonicalised (a loaded one is kept as it was). A member
   * function: Qt::UniqueConnection, which hookOtpField() needs because it
   * runs more than once for the same field, is refused for a lambda.
   */
  void markOtpFieldEdited();
  /**
   * @brief Rewrite the OTP field as a canonical otpauth URI.
   *
   * A value that cannot be parsed is left exactly as the user typed it, so
   * nothing is silently destroyed.
   */
  void normalizeOtpField();

  QScopedPointer<Ui::PasswordDialog> ui;
  PasswordConfiguration m_passConfig;
  Pass *m_pass{nullptr};
  QStringList m_fields;
  QString m_file;
  /// Absolute store root while a new entry is being named; empty otherwise.
  QString m_storeRoot;
  /// Folder chosen when the dialog opened; the template default follows the
  /// picker, so this is what setAvailableTemplates() was computed for.
  QString m_newEntryFolder;

  /**
   * @brief The name the user typed, resolved against the picked folder.
   * @param problem Receives a translated reason when the name is unusable.
   * @return The relative entry path, or an empty string on a problem.
   */
  auto resolveNewEntry(QString *problem) const -> QString;
  void validateNewEntry();
  /**
   * @brief Give a `key: value` field a new key (#132). Refused, with the
   *        reason in the status label, when another field has that name.
   */
  void renameField(QLineEdit *line, FieldLabel *label, const QString &to);
  /**
   * @brief Drop a `key: value` field and its row from the form. The widgets
   * are deleted once control is back in the event loop: the request comes
   * from the line's own trailing action or its label's context menu, both
   * still on the stack.
   */
  void removeField(QLineEdit *line);
  bool m_templating{};
  bool m_allFields{};
  bool m_isNew{};
  /// True once the existing entry's decrypted content has been loaded, so
  /// on_accepted() can refuse to overwrite it with empty fields before the
  /// asynchronous Show completes.
  bool m_contentLoaded{};
  /// Last pwgen mode passed to usePwgen(), so the policy can be re-applied
  /// after setEditorEnabled() re-enables the character-set widgets.
  bool m_usePwgen{};
  QList<QLineEdit *> m_templateLines;
  /// Fields that came from the entry rather than the template; their names
  /// are the user's, so their labels are FieldLabels that can be renamed.
  QList<QLineEdit *> m_otherLines;
  QHash<QString, QStringList> m_availableTemplates;
  QString m_currentTemplateName;
  /// Ctrl+T, created once templates are available; owned by the dialog.
  QShortcut *m_templateShortcut{nullptr};
  /// Warning indicator shown inside the OTP field; owned by that field.
  ///
  /// QPointer, not a raw pointer: the QAction is parented to the QLineEdit, and
  /// setPassword() deletes the m_otherLines widgets before calling
  /// hookOtpField(), so a raw pointer would already dangle there.
  QPointer<QAction> m_otpWarning;
  /// True once the user has typed in the OTP field. Only then may a value that
  /// is not already an otpauth URI be rewritten, so untouched data survives.
  bool m_otpFieldEdited{false};

  void applyTemplate(const QString &templateName);
  void setupTemplateBox();
};

#endif // SRC_PASSWORDDIALOG_H_
