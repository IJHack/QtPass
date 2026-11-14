// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief The first-run wizard and its four pages: programs, key, store and
 * summary.
 */
#ifndef SRC_FIRSTRUNWIZARD_H_
#define SRC_FIRSTRUNWIZARD_H_

#include "appsettings.h"
#include "userinfo.h"
#include <QList>
#include <QString>
#include <QWizard>
#include <QWizardPage>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * @class FirstRunWizard
 * @ingroup ui
 * @brief The first start of QtPass: find GnuPG (and pass and Git), make sure
 * there is a secret key, pick or create the password store. Replaces the
 * chain of message boxes ConfigDialog::wizard() used to run before showing
 * the whole configuration dialog.
 *
 * The wizard works on a copy of the settings and writes it back on Finish,
 * after it has created and initialised the store when that was needed
 * (ProfileInit). Cancel leaves the settings as they were; the caller keeps
 * asking until the configuration is usable or the user gives up.
 */
class FirstRunWizard : public QWizard {
  Q_OBJECT

public:
  /**
   * @brief Build the wizard from the current settings.
   * @param parent The main window.
   */
  explicit FirstRunWizard(QWidget *parent = nullptr);

  /**
   * @brief The settings as the pages have edited them so far.
   * @return A copy.
   */
  auto settings() const -> AppSettings { return m_settings; }

  /**
   * @brief The secret keys gpg lists for @p gpgExecutable.
   * @param gpgExecutable The gpg binary.
   * @return The keys, or an empty list when gpg cannot run.
   */
  static auto secretKeys(const QString &gpgExecutable) -> QList<UserInfo>;

  /**
   * @brief Whether @p path points at an initialised store.
   * @param path A directory.
   * @return true when it holds a `.gpg-id`.
   */
  static auto isStore(const QString &path) -> bool;

  /// Page ids, in order.
  enum Page {
    IntroPage,    ///< What QtPass is and what the wizard will do.
    ProgramsPage, ///< ProgramsWizardPage: where gpg, pass and git are.
    KeyPage,      ///< KeyWizardPage: the secret keys, with generation.
    StorePage,    ///< StoreWizardPage: the store folder and the Git choice.
    DonePage      ///< DoneWizardPage: summary and two preferences.
  };

  /**
   * @brief Write the settings and, when the chosen store has no `.gpg-id`
   * yet, create and initialise it for the chosen keys first.
   */
  void accept() override;

private:
  /**
   * @brief Make @p store a password store for the ticked keys: create the
   * folder when it is missing, write and sign its `.gpg-id`, and set up Git
   * when chosen. On failure what was written is taken back, so a second
   * Finish runs the whole initialisation again.
   * @param store The folder, cleaned.
   * @return Whether the store is ready (the user was told otherwise).
   */
  auto initialiseStore(const QString &store) -> bool;
  /**
   * @brief Give an existing store the Git repository it was chosen to have.
   * @param store The folder, cleaned.
   * @return Whether the repository is ready (the user was told otherwise).
   */
  auto initialiseGit(const QString &store) -> bool;

  friend class ProgramsWizardPage;
  friend class KeyWizardPage;
  friend class StoreWizardPage;
  friend class DoneWizardPage;

  /// The pages' working copy of the settings, loaded in the constructor and
  /// saved by accept(); the pages edit it through their friend access.
  AppSettings m_settings;
  /// The secret keys as listed on the key page; the enabled ones become the
  /// recipients of a new store.
  QList<UserInfo> m_keys;
};

/**
 * @class ProgramsWizardPage
 * @ingroup ui
 * @brief Where gpg, pass and git are; gpg is required, the rest optional. A
 * stored path that no longer runs is replaced by what PATH has.
 */
class ProgramsWizardPage : public QWizardPage {
  Q_OBJECT

public:
  /**
   * @brief Build the page.
   * @param wizard Owner.
   */
  explicit ProgramsWizardPage(FirstRunWizard *wizard);
  /**
   * @brief Fill the fields from the settings (autodetection already ran).
   */
  void initializePage() override;
  /**
   * @brief Whether the gpg path points at something that can run.
   * @return true when gpg exists (or is a `wsl` command).
   */
  auto isComplete() const -> bool override;
  /**
   * @brief Store the paths and the pass/Git choices in the settings.
   * @return Always true; isComplete() gates Next.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard; ///< Owner; its settings are what the page edits.
  QLineEdit *m_gpg;         ///< The gpg executable; required.
  QLineEdit *m_git;         ///< The git executable; optional.
  QLineEdit *m_pass;        ///< The pass executable; optional.
  QCheckBox *m_usePass;     ///< Run pass instead of gpg/git; needs m_pass.
  QLabel *m_gpgStatus;      ///< Under the gpg field: why it cannot be used.

  /**
   * @brief After an edit: say under the gpg field when it is missing or not
   * executable, allow "use pass" only for a runnable pass (unticking it
   * otherwise), and re-evaluate isComplete().
   */
  void updateStatus();
};

/**
 * @class KeyWizardPage
 * @ingroup ui
 * @brief The secret keys gpg knows; a new pair can be generated here. The
 * enabled ones become the recipients of a store that still has to be
 * initialised.
 */
class KeyWizardPage : public QWizardPage {
  Q_OBJECT

public:
  /**
   * @brief Build the page.
   * @param wizard Owner.
   */
  explicit KeyWizardPage(FirstRunWizard *wizard);
  /**
   * @brief List the secret keys for the gpg chosen on the previous page.
   */
  void initializePage() override;
  /**
   * @brief Always passable: an existing store needs no recipient choice
   * here (and its key may not be imported yet); the store page insists on a
   * ticked key when it has to initialise.
   * @return true.
   */
  auto isComplete() const -> bool override;
  /**
   * @brief Remember which keys are enabled.
   * @return Always true.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard; ///< Owner; holds the key list this page fills.
  QListWidget *m_list;      ///< One checkable row per secret key.
  QPushButton *m_generate;  ///< Opens KeygenDialog.
  QLabel *m_status;         ///< Says so when gpg has no secret key yet.

  /**
   * @brief List the secret keys of the chosen gpg into the wizard and the
   * widget, ticking every usable key and greying out expired or invalid
   * ones.
   */
  void reload();
  /**
   * @brief Run KeygenDialog on a private ImitatePass built from the wizard's
   * settings, so the chosen gpg is used and nothing is saved, then reload().
   */
  void generate();
};

/**
 * @class StoreWizardPage
 * @ingroup ui
 * @brief The folder the passwords live in: an existing store is used as-is,
 * a folder without `.gpg-id` (or no folder) is initialised on Finish. The
 * Git box lives here because its sensible default depends on the folder: on
 * for a repository or a store still to be created, off for an existing
 * store that is no repository.
 */
class StoreWizardPage : public QWizardPage {
  Q_OBJECT

public:
  /**
   * @brief Build the page.
   * @param wizard Owner.
   */
  explicit StoreWizardPage(FirstRunWizard *wizard);
  /**
   * @brief Show the configured store path.
   */
  void initializePage() override;
  /**
   * @brief Whether a path is given and, for a folder that is not a store
   * yet, at least one key was ticked to encrypt it to.
   * @return true when Finish can do its job.
   */
  auto isComplete() const -> bool override;
  /**
   * @brief Store the path and the Git choice.
   * @return Always true.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard; ///< Owner; its settings get the path and flag.
  QLineEdit *m_path;        ///< The store folder, in native separators.
  QCheckBox *m_useGit;      ///< Keep the store under Git; off without git.
  QLabel *m_status;         ///< What Finish will do with the folder.

  /**
   * @brief How many keys are ticked on the key page.
   * @return The number of enabled entries in the wizard's key list.
   */
  auto recipients() const -> int;
  /**
   * @brief After an edit: describe the folder (existing store with its
   * entry count, empty folder, not a store yet, missing) and ask for a
   * ticked key when it has to be initialised, then re-evaluate isComplete().
   */
  void updateStatus();
};

/**
 * @class DoneWizardPage
 * @ingroup ui
 * @brief A summary and the two preferences worth asking on day one.
 */
class DoneWizardPage : public QWizardPage {
  Q_OBJECT

public:
  /**
   * @brief Build the page.
   * @param wizard Owner.
   */
  explicit DoneWizardPage(FirstRunWizard *wizard);
  /**
   * @brief Fill the summary from the settings.
   */
  void initializePage() override;
  /**
   * @brief Store the preferences.
   * @return Always true.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard;  ///< Owner; its settings get the preferences.
  QLabel *m_summary;         ///< Store, what Finish does with it, gpg, mode.
  QCheckBox *m_hidePassword; ///< Show the password line as dots; on.
  QCheckBox *m_trayIcon;     ///< Tray icon; hidden when there is no tray.
};

#endif // SRC_FIRSTRUNWIZARD_H_
