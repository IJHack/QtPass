// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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
  enum Page { IntroPage, ProgramsPage, KeyPage, StorePage, DonePage };

  /**
   * @brief Write the settings and, when the chosen store has no `.gpg-id`
   * yet, create and initialise it for the chosen keys first.
   */
  void accept() override;

private:
  friend class ProgramsWizardPage;
  friend class KeyWizardPage;
  friend class StoreWizardPage;
  friend class DoneWizardPage;

  AppSettings m_settings;
  /// The secret keys as listed on the key page; the enabled ones become the
  /// recipients of a new store.
  QList<UserInfo> m_keys;
};

/**
 * @class ProgramsWizardPage
 * @brief Where gpg, pass and git are; gpg is required, the rest optional.
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
  FirstRunWizard *m_wizard;
  QLineEdit *m_gpg;
  QLineEdit *m_git;
  QLineEdit *m_pass;
  QCheckBox *m_usePass;
  QCheckBox *m_useGit;
  QLabel *m_gpgStatus;

  void updateStatus();
};

/**
 * @class KeyWizardPage
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
   * @brief Whether at least one key is listed and enabled.
   * @return true when a store can be encrypted to something.
   */
  auto isComplete() const -> bool override;
  /**
   * @brief Remember which keys are enabled.
   * @return Always true.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard;
  QListWidget *m_list;
  QPushButton *m_generate;
  QLabel *m_status;

  void reload();
  void generate();
};

/**
 * @class StoreWizardPage
 * @brief The folder the passwords live in: an existing store is used as-is,
 * a folder without `.gpg-id` (or no folder) is initialised on Finish.
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
   * @brief Whether a path is given.
   * @return true when the field is not empty.
   */
  auto isComplete() const -> bool override;
  /**
   * @brief Store the path.
   * @return Always true.
   */
  auto validatePage() -> bool override;

private:
  FirstRunWizard *m_wizard;
  QLineEdit *m_path;
  QLabel *m_status;

  void updateStatus();
};

/**
 * @class DoneWizardPage
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
  FirstRunWizard *m_wizard;
  QLabel *m_summary;
  QCheckBox *m_hidePassword;
  QCheckBox *m_trayIcon;
};

#endif // SRC_FIRSTRUNWIZARD_H_
