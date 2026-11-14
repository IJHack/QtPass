// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Dialog for creating or editing one password entry, including the
 * generator, template fields, OTP handling and the asynchronous save.
 */
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
class SecretLineEdit;
class QShortcut;
class QWidget;

/**
 * @class PasswordDialog
 * @ingroup ui
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
  /**
   * @brief Hides the window before QDialog tears it down, so the focus-out
   * emitted on hiding cannot reach slots of a half-destroyed dialog.
   */
  ~PasswordDialog() override;

  /**
   * @brief OK: for a new entry, resolve and check the name first and stay
   * open when it is not usable; then insert the entry and close as accepted
   * only once the insert has succeeded.
   *
   * The insert is asynchronous. Until it reports back the dialog stays open
   * with its fields locked; when it fails (a missing recipient key, an
   * unusable `.gpg-id`, a refused path) the fields come back with what was
   * typed and the reason is shown, instead of the entry being lost with the
   * closed dialog.
   */
  void accept() override;

  /**
   * @brief Cancel, Esc and the window's close button. While an insert is in
   * flight they ask first: closing then means a failure can no longer hand
   * back what was typed, but a save that never reports must not leave
   * quitting as the only way out.
   */
  void reject() override;

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

signals:
  /**
   * @brief Text was copied out of the password, a field or the body, marked
   * as a secret: for ClipboardManager::trackCopied(), so autoclear clears it.
   * @param text The text copied.
   */
  void copied(const QString &text);

public slots:
  /**
   * @brief Cycle to next template (Ctrl+T).
   */
  void cycleTemplate();

  /**
   * @brief Populate the dialog from a decrypted entry (Pass::finishedShow).
   *
   * Entries with more than FileContent::kMaxDisplayLines lines are not loaded
   * at all; the status label says so and the editor stays locked.
   * @param output Decrypted content of the entry.
   * @param file The entry the output belongs to; output for any other entry
   *        than the one being edited is ignored. Empty means unspecified.
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
  /**
   * @brief "Generate" button: asks the backend for a password of the chosen
   * length from the selected character set (Pass::generatePassword, which
   * runs pwgen when that mode is on) and puts it in the password field. An
   * empty result leaves the field alone.
   */
  void on_createPasswordButton_clicked();
  /**
   * @brief Connected to QDialog::rejected: blanks the password and field
   * widgets so the decrypted secret does not linger in a dialog that is
   * being discarded.
   */
  void on_rejected();

  /**
   * @brief The insert started by accept() succeeded: close as accepted.
   */
  void onInsertFinished();
  /**
   * @brief The insert started by accept() failed: unlock the fields, which
   * still hold what was typed, and show why.
   * @param err The reason reported by the backend.
   * @param written true when the entry was written and only a later step
   *        (git) failed; a new entry then closes the dialog.
   */
  void onInsertFailed(const QString &err, bool written);

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
   * @brief Forwards the line edit's SecretLineEdit::copied to this dialog's
   * copied(), so every copy out of any field reaches ClipboardManager.
   * @param line A password or field editor just created.
   */
  void reportCopies(SecretLineEdit *line);
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
   * @brief Start inserting what the dialog holds and wait for the result.
   */
  void save();
  /**
   * @brief Lock or unlock the dialog while an insert is in flight.
   * @param saving true while waiting for the backend.
   */
  void setSaving(bool saving);
  /**
   * @brief Whether the insert report now arriving is this dialog's own.
   *
   * False when the dialog is not saving. Otherwise it lets the reports of the
   * inserts that were already pending when save() ran pass first (one per
   * call), so an earlier dialog's result does not close or fail this one.
   * @return true when the arriving report belongs to this dialog's insert.
   */
  auto isOwnInsertReport() -> bool;
  /**
   * @brief Say why the entry was not saved and let the user try again; the
   * fields keep what was typed.
   * @param err The reason reported by the backend.
   */
  void showNotSaved(const QString &err);
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
   * @brief Remember, on the line edit that sent textEdited(), that the user
   * typed in it: only such a field has a bare secret canonicalised, a loaded
   * value is kept as it was. The mark lives on the widget, not on the dialog,
   * so typing into a field that used to be the OTP one (renamed away) says
   * nothing about the field that is the OTP one now. A member function:
   * Qt::UniqueConnection, which hookOtpField() needs because it runs more
   * than once for the same field, is refused for a lambda (a debug build
   * asserts, a release build makes no connection).
   */
  void markOtpFieldEdited();
  /**
   * @brief Rewrite the OTP field as a canonical otpauth URI.
   *
   * A value that cannot be parsed is left exactly as the user typed it, so
   * nothing is silently destroyed.
   */
  void normalizeOtpField();

  /** @brief Generated form (`passworddialog.ui`); owned. */
  QScopedPointer<Ui::PasswordDialog> ui;
  /** @brief Generator settings: character sets and default length. */
  PasswordConfiguration m_passConfig;
  /** @brief Backend for Show, Insert and generatePassword; not owned. */
  Pass *m_pass{nullptr};
  /** @brief Field names of the current template, in template order. */
  QStringList m_fields;
  /** @brief Entry path relative to the store, without `.gpg`; empty for a
   * new entry until accept() resolves the typed name. */
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
  /**
   * @brief Re-runs resolveNewEntry() as the user types or picks a folder:
   * shows the problem (if any) in the status label and enables OK only for a
   * usable name.
   */
  void validateNewEntry();
  /**
   * @brief Give a `key: value` field a new key (#132). Refused, with the
   *        reason in the status label, when another field has that name.
   *        Re-runs hookOtpField(), as the field may have become or stopped
   *        being the OTP one.
   * @param line The field's editor; its object name is the key.
   * @param label The field's label, updated to show the new key.
   * @param to The new key.
   */
  void renameField(QLineEdit *line, FieldLabel *label, const QString &to);
  /**
   * @brief Drop a `key: value` field and its row from the form. The widgets
   * are deleted once control is back in the event loop: the request comes
   * from the line's own trailing action or its label's context menu, both
   * still on the stack.
   * @param line The field's editor, one of m_otherLines.
   */
  void removeField(QLineEdit *line);
  /** @brief Whether template fields get widgets of their own (the
   * "Use template" setting). */
  bool m_templating{};
  /** @brief Whether every `key: value` line becomes a field (the "Template
   * all fields" setting), as given to FileContent::parse(). */
  bool m_allFields{};
  /** @brief True when creating an entry, false when editing one; decides
   * whether the name row and the decrypt-before-edit lock apply. */
  bool m_isNew{};
  /// True once the existing entry's decrypted content has been loaded, so
  /// save() can refuse to overwrite it with empty fields before the
  /// asynchronous Show completes.
  bool m_contentLoaded{};
  /// True from accept() starting an insert until the backend reports back.
  bool m_saving{};
  /// Inserts already pending when save() ran, whose reports must pass before
  /// one is taken as this dialog's own (counted down in isOwnInsertReport()).
  int m_insertsAhead{};
  /// Last pwgen mode passed to usePwgen(), so the policy can be re-applied
  /// after setEditorEnabled() re-enables the character-set widgets.
  bool m_usePwgen{};
  /// Editors for the template's fields, in template order; rebuilt by
  /// setTemplate(). Their labels are plain QLabels: the names are fixed.
  QList<QLineEdit *> m_templateLines;
  /// Fields that came from the entry rather than the template; their names
  /// are the user's, so their labels are FieldLabels that can be renamed.
  QList<QLineEdit *> m_otherLines;
  /// Named templates from `.templates`, name to field list, as given to
  /// setAvailableTemplates(); empty when the store has none.
  QHash<QString, QStringList> m_availableTemplates;
  /// Name of the template currently applied; cycleTemplate() steps from it.
  QString m_currentTemplateName;
  /// Ctrl+T, created once templates are available; owned by the dialog.
  QShortcut *m_templateShortcut{nullptr};
  /// Warning indicator shown inside the OTP field; owned by that field.
  ///
  /// QPointer, not a raw pointer: the QAction is parented to the QLineEdit, and
  /// setPassword() deletes the m_otherLines widgets before calling
  /// hookOtpField(), so a raw pointer would already dangle there.
  QPointer<QAction> m_otpWarning;
  /// The line edit hookOtpField() last wired up, so its signals can be
  /// disconnected when another field becomes the OTP one (a rename, a
  /// removal): a stale editingFinished from the old field must not rewrite
  /// the new one.
  QPointer<QLineEdit> m_otpLine;

  /**
   * @brief Switch to a named template: records it, selects it in the combo
   * box and rebuilds the field rows through setTemplate(). Unknown names
   * are ignored, and nothing changes while a save is in flight.
   * @param templateName A key of m_availableTemplates.
   */
  void applyTemplate(const QString &templateName);
  /**
   * @brief Hides the template row until setAvailableTemplates() fills it and
   * wires the combo box so that a user selection calls applyTemplate().
   */
  void setupTemplateBox();
};

#endif // SRC_PASSWORDDIALOG_H_
