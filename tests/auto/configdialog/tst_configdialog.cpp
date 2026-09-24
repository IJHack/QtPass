// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>
#include <functional>
#include <memory>

#include "../../../src/configdialog.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"
#include "../testpass.h"
#include "../testsettings.h"

// QMessageBox shows no window title on macOS (Apple's guidelines) and Qt
// leaves windowTitle() empty there, so a title is compared everywhere else
// and the box's text carries the assertion on macOS.
#ifdef Q_OS_MACOS
#define COMPARE_BOX_TITLE(actual, expected)                                    \
  do {                                                                         \
    Q_UNUSED(actual)                                                           \
    Q_UNUSED(expected)                                                         \
  } while (false)
#else
#define COMPARE_BOX_TITLE(actual, expected) QCOMPARE(actual, expected)
#endif

namespace {
/**
 * @brief Write an executable /bin/sh script that stands in for an external
 * program. The script restores a sane PATH for its own tools, since the
 * suite narrows the process PATH to the directory holding these stand-ins.
 */
auto writeScript(const QString &path, const QByteArray &body) -> bool {
  QFile script(path);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  script.write("#!/bin/sh\nPATH=/usr/bin:/bin:/usr/local/bin\n");
  script.write(body);
  script.close();
  return script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                               QFile::ExeOwner);
}

/**
 * @brief Drives the modal dialogs a slot opens while it blocks in exec().
 *
 * A timer polls QApplication::activeModalWidget() from the nested event loop
 * and hands every modal to the handler exactly once; the handler closes it.
 * seen() counts the modals that came by, so a test can also assert that none
 * did.
 *
 * accept()/reject() hide a dialog synchronously, so meeting a driven modal
 * on a later tick means the handler did not close it (QFileDialog refusing a
 * name, UsersDialog with nothing ticked). It is rejected rather than left to
 * block the slot under test forever, and stuck() names it so the test fails
 * with a reason. A handler that closes its modal from a posted event marks
 * it "tst_pending" until that event has run.
 */
class ModalDriver {
public:
  explicit ModalDriver(std::function<void(QWidget *)> handler)
      : m_handler(std::move(handler)) {
    m_timer.setInterval(10);
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this]() {
      QWidget *modal = QApplication::activeModalWidget();
      if (modal == nullptr) {
        return;
      }
      if (modal->property("tst_driven").toBool()) {
        if (modal->property("tst_pending").toBool()) {
          return;
        }
        m_stuck << QString::fromLatin1(modal->metaObject()->className());
        if (auto *dialog = qobject_cast<QDialog *>(modal)) {
          dialog->reject();
        } else {
          modal->hide();
        }
        return;
      }
      modal->setProperty("tst_driven", true);
      ++m_seen;
      m_handler(modal);
    });
    m_timer.start();
  }
  auto seen() const -> int { return m_seen; }
  auto stuck() const -> const QStringList & { return m_stuck; }

private:
  QTimer m_timer;
  std::function<void(QWidget *)> m_handler;
  int m_seen = 0;
  QStringList m_stuck;
};

/**
 * @brief Put the profiles and the settings back the way a test found them.
 */
struct SettingsRestorer {
  Profiles profiles = QtPassSettings::getProfiles();
  AppSettings settings = QtPassSettings::load();
  ~SettingsRestorer() {
    QtPassSettings::setProfiles(profiles);
    QtPassSettings::save(settings);
  }
};
} // namespace

/**
 * @class tst_configdialog
 * @brief Widget tests for ConfigDialog's `useX(bool)` setting-loaders.
 *
 * ConfigDialog is the main Settings dialog and is fed by QtPassSettings on
 * construction. Most of its public-facing behaviour lives in a family of
 * `useX(bool)` methods that read settings into checkbox state; those are
 * pure widget-state setters and testable in isolation by passing nullptr
 * as the parent MainWindow.
 *
 * The slots that open other dialogs (the browse buttons, "Generate key", the
 * SSH_AUTH_SOCK warnings and the new-profile initialisation) are driven
 * through ModalDriver against stand-in programs on a narrowed PATH, so no
 * real gpg, git or pass ever runs.
 *
 * Not coverable here: the Wayland-only "always on top" note, the clipboard
 * without a primary selection, and everything behind
 * QSystemTrayIcon::isSystemTrayAvailable() (the suite runs offscreen).
 */
class tst_configdialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void constructionDoesNotCrash();
  void useSelectionTogglesCheckbox();
  void autoclearIsOneSpinBoxWhereZeroIsNever();
  void autoclearPanelIsOneSpinBoxWhereZeroIsNever();
  void autoclearBoxesAreWideEnoughForNever();
  void useGitTogglesCheckbox();
  void useOtpTogglesCheckbox();
  void useGrepSearchTogglesCheckbox();
  void usePwgenTogglesCheckbox();
  void templatingIsOneChoiceOfThree();
  void viewTogglesAreNotInTheDialog();
  void useTrayIconTogglesCheckbox();
  void useQrencodeTogglesCheckbox();
  void setPwgenPathSetsLineEdit();
  void setPwgenPathEmptyDisablesPwgenCheckbox();
  void setAndGetPasswordConfigurationRoundTrip();
  void customCharsetRoundTrip();
  void customCharsetPreservedWhenBuiltinSelected();
  void addProfileSelectsTheNewOne();
  void profileFormEditsTheSelectedProfile();
  void okWaitsForValidProfiles();
  void signingKeyMustBeAFullFingerprint();
  void activeProfileFollowsRenameAndDeletion();
  void fieldLabelsHaveBuddies();
  void browseButtonsAreNamed();
  void dialogCanShrinkBelowItsOldMinimum();
  void sectionHeadersAreGroupBoxes();
  void pagesFollowTheSidebar();
  void invalidConfigurationOpensPrograms();
  void acceptRoundTripsEveryOwnedSetting();
  void acceptSavesTheGlobalAutoPushAndAutoPull();
  void qrencodeMissingDisablesTheCheckbox();
  void qrencodeConfiguredExecutableEnablesTheCheckbox();
  void qrencodeOnPathIsStored();
  void trayIconCheckboxGatesItsCompanions();
  void autodetectFindsTheProgramsOnPath();
  void backendRadioButtonsToggleTheGroupBoxes();
  void charsetSelectorFollowsTheSavedSets();
  void browseButtonsTakeTheChosenFile();
  void browseButtonsLeaveTheFieldWhenCancelled();
  void folderBrowseButtonsTakeTheChosenFolder();
  void folderBrowseCancelLeavesTheField();
  void generateKeyOpensTheKeygenDialog();
  void deleteWithoutASelectionWarns();
  void sshAuthSockOverrideWarnsWhenMissing();
  void sshAuthSockOverrideWarnsForARegularFile();
  void sshAuthSockOverrideWarnsWhenUnreadable();
  void newProfileDirectoryQuestionCanBeDeclined();
  void newProfileDirectoryCreationFailureIsReported();
  void newProfileWithAGpgIdNeedsNoInitialisation();
  void newProfileRecipientsCancelSkipsInitialisation();
  void newProfileIsInitialisedWithTheChosenRecipients();
  void newProfileInitialisationFailureIsReported();

private:
  /// Stand-in programs (shell scripts) the narrowed PATH consists of.
  QTemporaryDir m_bin;
  QString m_gpg, m_git, m_pass, m_pwgen;

  auto browse(ConfigDialog &dialog, const char *button, const QString &choose)
      -> QString;
  auto dialogWithNewProfile(const QString &store, const QString &name,
                            const QString &path, const QString &signingKey)
      -> std::unique_ptr<ConfigDialog>;
};

/**
 * @brief Isolate the settings, pin Qt's own dialogs and lay out the stand-in
 *        programs.
 *
 * ModalDriver finds the dialogs a slot opens through
 * QApplication::activeModalWidget(). A native (platform-theme) file dialog
 * or message box never enters that modal-widget stack, so with a desktop
 * theme such as gtk3 the driver would wait forever; the widget-based dialogs
 * behave the same on every platform.
 */
void tst_configdialog::initTestCase() {
  isolateTestSettings();
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#ifndef Q_OS_WIN
  // Narrow PATH to a directory of stand-ins: pass, gpg (no gpg2, so the
  // fallback lookup is what finds it), git and pwgen, and no qrencode. The
  // dialog then never sees or runs the machine's real programs, and the
  // autodetect and availability probes have a known answer.
  QVERIFY2(m_bin.isValid(), qPrintable(m_bin.errorString()));
  const QDir bin(m_bin.path());
  m_gpg = bin.filePath(QStringLiteral("gpg"));
  m_git = bin.filePath(QStringLiteral("git"));
  m_pass = bin.filePath(QStringLiteral("pass"));
  m_pwgen = bin.filePath(QStringLiteral("pwgen"));
  QByteArray gpg = "case \"$*\" in\n";
  gpg += "*--version*) echo \"gpg (GnuPG) 2.4.0\"; exit 0 ;;\n";
  gpg += "*--list-secret-keys*) exit 0 ;;\n";
  gpg += "*--list-keys*) cat <<'LISTING'\n";
  gpg += kColonListing;
  gpg += "LISTING\nexit 0 ;;\nesac\nexit 0\n";
  QVERIFY2(writeScript(m_gpg, gpg), qPrintable("cannot write " + m_gpg));
  for (const QString &tool : {m_git, m_pass, m_pwgen}) {
    QVERIFY2(writeScript(tool, "exit 0\n"), qPrintable("cannot write " + tool));
  }
  qputenv("PATH", QFile::encodeName(m_bin.path()));
#endif
}

void tst_configdialog::constructionDoesNotCrash() {
  ConfigDialog dialog(nullptr);
  // Reaching this line means the constructor's setting-load + widget-
  // wiring path completed without a segfault.
  Q_UNUSED(dialog.isModal());
}

/**
 * @brief useSelection toggles the X11 primary-selection checkbox.
 */
void tst_configdialog::useSelectionTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxSelection"));
  QVERIFY2(cb != nullptr, "checkBoxSelection widget must exist");

  dialog.useSelection(true);
  QVERIFY2(cb->isChecked(),
           "useSelection(true) should check checkBoxSelection");
  dialog.useSelection(false);
  QVERIFY2(!cb->isChecked(),
           "useSelection(false) should uncheck checkBoxSelection");
}

namespace {
/// Seed the persisted settings, open the dialog, and hand back its spin box.
auto autoclearBox(ConfigDialog &dialog, const char *name) -> QSpinBox * {
  return dialog.findChild<QSpinBox *>(QString::fromLatin1(name));
}

/// Open a dialog on @p seed, set @p box to @p value, accept, reload.
auto roundTripAutoclear(const AppSettings &seed, const char *box, int value)
    -> AppSettings {
  QtPassSettings::save(seed);
  ConfigDialog dialog(nullptr);
  autoclearBox(dialog, box)->setValue(value);
  dialog.accept();
  return QtPassSettings::load();
}
} // namespace

/**
 * @brief Clipboard autoclear is one control: the spin box shows "Never" at
 *        0, loads an off setting as 0 whatever delay was stored, and saves
 *        0 as off and anything else as on with that delay.
 */
void tst_configdialog::autoclearIsOneSpinBoxWhereZeroIsNever() {
  SettingsRestorer restorer;
  AppSettings seed = QtPassSettings::load();
  seed.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  seed.useAutoclear = false;
  seed.autoclearSeconds = 30;
  QtPassSettings::save(seed);
  {
    ConfigDialog dialog(nullptr);
    QSpinBox *box = autoclearBox(dialog, "spinBoxAutoclearSeconds");
    QVERIFY(box != nullptr);
    QCOMPARE(box->value(), 0);
    QCOMPARE(box->text(), QStringLiteral("Never"));
    QVERIFY(dialog.findChild<QCheckBox *>(
                QStringLiteral("checkBoxAutoclear")) == nullptr);
  }
  AppSettings on = roundTripAutoclear(seed, "spinBoxAutoclearSeconds", 12);
  QVERIFY(on.useAutoclear);
  QCOMPARE(on.autoclearSeconds, 12);
  AppSettings off = roundTripAutoclear(on, "spinBoxAutoclearSeconds", 0);
  QVERIFY(!off.useAutoclear);
}

/**
 * @brief The content panel's autoclear folds the same way.
 */
void tst_configdialog::autoclearPanelIsOneSpinBoxWhereZeroIsNever() {
  SettingsRestorer restorer;
  AppSettings seed = QtPassSettings::load();
  seed.useAutoclearPanel = true;
  seed.autoclearPanelSeconds = 20;
  QtPassSettings::save(seed);
  {
    ConfigDialog dialog(nullptr);
    QCOMPARE(autoclearBox(dialog, "spinBoxAutoclearPanelSeconds")->value(), 20);
  }
  AppSettings off = roundTripAutoclear(seed, "spinBoxAutoclearPanelSeconds", 0);
  QVERIFY(!off.useAutoclearPanel);
  AppSettings on = roundTripAutoclear(off, "spinBoxAutoclearPanelSeconds", 7);
  QVERIFY(on.useAutoclearPanel);
  QCOMPARE(on.autoclearPanelSeconds, 7);
}

/**
 * @brief useGit toggles the "use git" checkbox.
 */
void tst_configdialog::useGitTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseGit"));
  QVERIFY2(cb != nullptr, "checkBoxUseGit widget must exist");

  dialog.useGit(true);
  QVERIFY2(cb->isChecked(), "useGit(true) should check checkBoxUseGit");
  dialog.useGit(false);
  QVERIFY2(!cb->isChecked(), "useGit(false) should uncheck checkBoxUseGit");
}

/**
 * @brief useOtp toggles the OTP support checkbox.
 */
void tst_configdialog::useOtpTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseOtp"));
  QVERIFY2(cb != nullptr, "checkBoxUseOtp widget must exist");
  // OTP is generated in-process, so the checkbox is no longer gated on
  // probing for the pass-otp extension, nor hidden on Windows.
  QVERIFY2(cb->isEnabled(),
           "checkBoxUseOtp must not be disabled by an availability probe");

  dialog.useOtp(true);
  QVERIFY2(cb->isChecked(), "useOtp(true) should check checkBoxUseOtp");
  dialog.useOtp(false);
  QVERIFY2(!cb->isChecked(), "useOtp(false) should uncheck checkBoxUseOtp");
}

/**
 * @brief useGrepSearch toggles the "content search" checkbox.
 */
void tst_configdialog::useGrepSearchTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseGrepSearch"));
  QVERIFY2(cb != nullptr, "checkBoxUseGrepSearch widget must exist");

  dialog.useGrepSearch(true);
  QVERIFY2(cb->isChecked(),
           "useGrepSearch(true) should check checkBoxUseGrepSearch");
  dialog.useGrepSearch(false);
  QVERIFY2(!cb->isChecked(),
           "useGrepSearch(false) should uncheck checkBoxUseGrepSearch");
}

/**
 * @brief usePwgen toggles the pwgen-generator checkbox — but only if a
 *        pwgen path is configured. With an empty pwgenPath, usePwgen(true)
 *        is intentionally clamped to false (you can't use a tool that
 *        isn't there).
 */
void tst_configdialog::usePwgenTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUsePwgen"));
  auto *pwgenPath = dialog.findChild<QLineEdit *>(QStringLiteral("pwgenPath"));
  QVERIFY2(cb != nullptr, "checkBoxUsePwgen widget must exist");
  QVERIFY2(pwgenPath != nullptr, "pwgenPath widget must exist");

  // First verify the empty-path branch: even usePwgen(true) leaves the
  // checkbox unchecked when pwgenPath is empty.
  pwgenPath->setText(QString());
  dialog.usePwgen(true);
  QVERIFY2(!cb->isChecked(),
           "usePwgen(true) with empty pwgenPath must stay unchecked");

  // Now with a non-empty path the value flows through.
  pwgenPath->setText(QStringLiteral("/usr/bin/pwgen"));
  dialog.usePwgen(true);
  QVERIFY2(cb->isChecked(),
           "usePwgen(true) with a configured pwgenPath should check "
           "checkBoxUsePwgen");
  dialog.usePwgen(false);
  QVERIFY2(!cb->isChecked(), "usePwgen(false) should uncheck checkBoxUsePwgen");
}

/**
 * @brief "Use template" and "Show all fields templated" are one choice of
 *        three (all fields only applies while templating is on, #1766). Each
 *        stored pair loads as its choice, each choice saves as its pair, and
 *        the template text is kept whichever is chosen.
 */
void tst_configdialog::templatingIsOneChoiceOfThree() {
  SettingsRestorer restorer;
  struct Case {
    bool useTemplate;
    bool allFields;
    int index;
  };
  for (const Case c :
       {Case{false, false, 0}, Case{true, false, 1}, Case{true, true, 2}}) {
    AppSettings seed = QtPassSettings::load();
    seed.useTemplate = c.useTemplate;
    seed.templateAllFields = c.allFields;
    seed.passTemplate = QStringLiteral("login\nurl");
    QtPassSettings::save(seed);
    ConfigDialog dialog(nullptr);
    auto *fields =
        dialog.findChild<QComboBox *>(QStringLiteral("comboBoxFields"));
    QCOMPARE(fields->currentIndex(), c.index);
    QCOMPARE(dialog
                 .findChild<QPlainTextEdit *>(
                     QStringLiteral("plainTextEditTemplate"))
                 ->isEnabled(),
             c.useTemplate);
  }
  for (const Case c :
       {Case{false, false, 0}, Case{true, false, 1}, Case{true, true, 2}}) {
    ConfigDialog dialog(nullptr);
    dialog.findChild<QComboBox *>(QStringLiteral("comboBoxFields"))
        ->setCurrentIndex(c.index);
    dialog.accept();
    const AppSettings s = QtPassSettings::load();
    QCOMPARE(s.useTemplate, c.useTemplate);
    QCOMPARE(s.templateAllFields, c.allFields);
    QCOMPARE(s.passTemplate, QStringLiteral("login\nurl"));
  }
}

/**
 * @brief The menu bar and the process output are toggled from the main
 *        window's Settings menu; the dialog neither shows them nor touches
 *        them when it saves.
 */
void tst_configdialog::viewTogglesAreNotInTheDialog() {
  SettingsRestorer restorer;
  AppSettings seed = QtPassSettings::load();
  seed.showMenuBar = true;
  seed.showProcessOutput = true;
  QtPassSettings::save(seed);
  ConfigDialog dialog(nullptr);
  QVERIFY(dialog.findChild<QCheckBox *>(
              QStringLiteral("checkBoxShowMenuBar")) == nullptr);
  QVERIFY(dialog.findChild<QCheckBox *>(
              QStringLiteral("checkBoxShowProcessOutput")) == nullptr);
  dialog.accept();
  const AppSettings s = QtPassSettings::load();
  QVERIFY(s.showMenuBar);
  QVERIFY(s.showProcessOutput);
}

void tst_configdialog::useTrayIconTogglesCheckbox() {
  if (!QSystemTrayIcon::isSystemTrayAvailable())
    QSKIP("system tray not available in this environment");
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseTrayIcon"));
  QVERIFY2(cb != nullptr, "checkBoxUseTrayIcon widget must exist");
  dialog.useTrayIcon(true);
  QVERIFY2(cb->isChecked(),
           "useTrayIcon(true) should check checkBoxUseTrayIcon");
  dialog.useTrayIcon(false);
  QVERIFY2(!cb->isChecked(),
           "useTrayIcon(false) should uncheck checkBoxUseTrayIcon");
}

void tst_configdialog::useQrencodeTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseQrencode"));
  QVERIFY2(cb != nullptr, "checkBoxUseQrencode widget must exist");
  dialog.useQrencode(true);
  QVERIFY2(cb->isChecked(),
           "useQrencode(true) should check checkBoxUseQrencode");
  dialog.useQrencode(false);
  QVERIFY2(!cb->isChecked(),
           "useQrencode(false) should uncheck checkBoxUseQrencode");
}

void tst_configdialog::setPwgenPathSetsLineEdit() {
  ConfigDialog dialog(nullptr);
  auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("pwgenPath"));
  QVERIFY2(pathEdit != nullptr, "pwgenPath widget must exist");
  dialog.setPwgenPath(QStringLiteral("/usr/bin/pwgen"));
  QCOMPARE(pathEdit->text(), QStringLiteral("/usr/bin/pwgen"));
}

void tst_configdialog::setPwgenPathEmptyDisablesPwgenCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUsePwgen"));
  QVERIFY2(cb != nullptr, "checkBoxUsePwgen widget must exist");
  dialog.setPwgenPath(QString());
  QVERIFY2(!cb->isChecked(), "setPwgenPath('') must uncheck checkBoxUsePwgen");
  QVERIFY2(!cb->isEnabled(), "setPwgenPath('') must disable checkBoxUsePwgen");
}

void tst_configdialog::setAndGetPasswordConfigurationRoundTrip() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.length = 32;
  cfg.selected = PasswordConfiguration::ALPHANUMERIC;
  dialog.setPasswordConfiguration(cfg);
  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(result.length, 32);
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::ALPHANUMERIC));
}

/**
 * @brief A CUSTOM charset survives the set/get round-trip.
 *
 * getPasswordConfiguration() only reads the character line edit while CUSTOM is
 * selected; this guards that path so a user-defined charset is not lost or
 * replaced by a builtin string on save.
 */
void tst_configdialog::customCharsetRoundTrip() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.selected = PasswordConfiguration::CUSTOM;
  cfg.Characters[PasswordConfiguration::CUSTOM] = QStringLiteral("abc123!@#");
  dialog.setPasswordConfiguration(cfg);
  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::CUSTOM));
  QCOMPARE(result.Characters[PasswordConfiguration::CUSTOM],
           QStringLiteral("abc123!@#"));
}

/**
 * @brief The custom charset survives while a builtin set is selected.
 *
 * This is the branch the fix is actually about: with a builtin (non-CUSTOM)
 * selection the line edit shows the builtin's characters, so
 * getPasswordConfiguration() must fall back to the retained custom charset
 * rather than reading the line edit and clobbering it with a builtin string.
 */
void tst_configdialog::customCharsetPreservedWhenBuiltinSelected() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.selected = PasswordConfiguration::CUSTOM;
  cfg.Characters[PasswordConfiguration::CUSTOM] = QStringLiteral("abc123!@#");
  dialog.setPasswordConfiguration(cfg);

  // Switch to a builtin selection while keeping the same custom charset.
  PasswordConfiguration builtin = dialog.getPasswordConfiguration();
  builtin.selected = PasswordConfiguration::ALPHANUMERIC;
  dialog.setPasswordConfiguration(builtin);

  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::ALPHANUMERIC));
  QCOMPARE(result.Characters[PasswordConfiguration::CUSTOM],
           QStringLiteral("abc123!@#"));
}

template <typename T> auto child(ConfigDialog &d, const char *name) -> T * {
  auto *w = d.findChild<T *>(QLatin1String(name));
  if (w == nullptr) {
    qFatal("missing widget %s", name);
  }
  return w;
}

/**
 * @brief Adding a profile appends it to the list, selects it and puts its
 * default name into the form, ready to be typed over.
 */
void tst_configdialog::addProfileSelectsTheNewOne() {
  struct ProfileRestorer {
    Profiles saved;
    ~ProfileRestorer() { QtPassSettings::setProfiles(saved); }
  } restorer{QtPassSettings::getProfiles()};

  Profiles profiles;
  for (const QString &name :
       {QStringLiteral("alpha"), QStringLiteral("beta"),
        QStringLiteral("gamma"), QStringLiteral("delta")}) {
    Profile profile;
    profile.path = "/store/" + name;
    profiles.insert(name, profile);
  }
  QtPassSettings::setProfiles(profiles);

  ConfigDialog dialog(nullptr);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("profileList"));
  auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("profileName"));
  QVERIFY(list != nullptr && name != nullptr);
  QCOMPARE(list->count(), 4);

  QVERIFY(QMetaObject::invokeMethod(&dialog, "on_addButton_clicked"));

  QCOMPARE(list->count(), 5);
  QCOMPARE(list->currentRow(), 4);
  QCOMPARE(list->currentItem()->text(), QStringLiteral("New profile"));
  QCOMPARE(name->text(), QStringLiteral("New profile"));
  QVERIFY2(name->hasSelectedText(), "the name is selected for typing over");
}

/**
 * @brief The form edits the selected profile in place: name, path, signing
 * key and the profile's own Git flags all end up in getProfiles(), and
 * switching profiles keeps each one's values.
 */
void tst_configdialog::profileFormEditsTheSelectedProfile() {
  struct ProfileRestorer {
    Profiles saved;
    ~ProfileRestorer() { QtPassSettings::setProfiles(saved); }
  } restorer{QtPassSettings::getProfiles()};

  Profiles profiles;
  Profile work;
  work.path = QStringLiteral("/store/work");
  work.useGit = true;
  work.autoPush = false;
  profiles.insert(QStringLiteral("work"), work);
  Profile home;
  home.path = QStringLiteral("/store/home");
  profiles.insert(QStringLiteral("home"), home);
  QtPassSettings::setProfiles(profiles);
  {
    AppSettings s = QtPassSettings::load();
    s.activeProfile = QStringLiteral("work");
    QtPassSettings::save(s);
  }

  ConfigDialog dialog(nullptr);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("profileList"));
  auto *name = child<QLineEdit>(dialog, "profileName");
  auto *path = child<QLineEdit>(dialog, "profilePath");
  auto *key = child<QLineEdit>(dialog, "profileSigningKey");
  auto *useGit = dialog.findChild<QCheckBox *>(QStringLiteral("profileUseGit"));
  auto *autoPush =
      dialog.findChild<QCheckBox *>(QStringLiteral("profileAutoPush"));
  QVERIFY(list != nullptr && useGit != nullptr && autoPush != nullptr);

  // The active profile is selected and shown; the list is alphabetical.
  QCOMPARE(list->currentItem()->text(), QStringLiteral("work"));
  QCOMPARE(path->text(), QStringLiteral("/store/work"));
  QVERIFY(useGit->isChecked());
  QVERIFY(!autoPush->isChecked());

  // Type into the form: the entry follows, the list shows the new name.
  name->setText(QStringLiteral("office"));
  emit name->textEdited(name->text());
  key->setText(QStringLiteral("ABCDEF0123456789ABCDEF0123456789ABCDEF01"));
  emit key->textEdited(key->text());
  autoPush->click();
  QCOMPARE(list->currentItem()->text(), QStringLiteral("office"));

  // Switch to the other profile and back: nothing leaks between them.
  list->setCurrentRow(0);
  QCOMPARE(name->text(), QStringLiteral("home"));
  QCOMPARE(path->text(), QStringLiteral("/store/home"));
  QVERIFY(key->text().isEmpty());
  path->setText(QStringLiteral("/store/home2"));
  emit path->textEdited(path->text());
  list->setCurrentRow(1);
  QCOMPARE(name->text(), QStringLiteral("office"));

  const Profiles edited = dialog.getProfiles();
  QCOMPARE(edited.keys(),
           (QStringList{QStringLiteral("home"), QStringLiteral("office")}));
  QCOMPARE(edited.value(QStringLiteral("home")).path,
           QStringLiteral("/store/home2"));
  QVERIFY2(!edited.value(QStringLiteral("home")).useGit.has_value(),
           "an untouched Git box leaves the profile without its own flag");
  const Profile office = edited.value(QStringLiteral("office"));
  QCOMPARE(office.path, QStringLiteral("/store/work"));
  QCOMPARE(office.signingKey,
           QStringLiteral("ABCDEF0123456789ABCDEF0123456789ABCDEF01"));
  QCOMPARE(office.useGit, std::optional<bool>(true));
  QCOMPARE(office.autoPush, std::optional<bool>(true));
}

/**
 * @brief OK stays off while a profile has no name, no path or a name another
 * profile already uses, and comes back once the problem is typed away.
 */
void tst_configdialog::okWaitsForValidProfiles() {
  struct ProfileRestorer {
    Profiles saved;
    ~ProfileRestorer() { QtPassSettings::setProfiles(saved); }
  } restorer{QtPassSettings::getProfiles()};

  Profiles profiles;
  Profile one;
  one.path = QStringLiteral("/store/one");
  profiles.insert(QStringLiteral("one"), one);
  profiles.insert(QStringLiteral("two"), one);
  QtPassSettings::setProfiles(profiles);
  {
    AppSettings s = QtPassSettings::load();
    s.activeProfile = QStringLiteral("two");
    QtPassSettings::save(s);
  }

  ConfigDialog dialog(nullptr);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("profileList"));
  auto *name = child<QLineEdit>(dialog, "profileName");
  auto *path = child<QLineEdit>(dialog, "profilePath");
  auto *ok = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))
                 ->button(QDialogButtonBox::Ok);
  QVERIFY(list != nullptr && ok != nullptr);
  QVERIFY(ok->isEnabled());

  name->setText(QStringLiteral("one"));
  emit name->textEdited(name->text());
  QVERIFY2(!ok->isEnabled(), "duplicate name");
  QVERIFY(name->toolTip().contains(QStringLiteral("already")));
  QVERIFY(list->currentItem()->background().color() == Qt::red);

  name->setText(QString());
  emit name->textEdited(name->text());
  QVERIFY2(!ok->isEnabled(), "empty name");

  name->setText(QStringLiteral("three"));
  emit name->textEdited(name->text());
  QVERIFY(ok->isEnabled());
  QVERIFY(list->currentItem()->background() == QBrush());

  path->setText(QString());
  emit path->textEdited(path->text());
  QVERIFY2(!ok->isEnabled(), "empty path");

  QVERIFY(QMetaObject::invokeMethod(&dialog, "on_deleteButton_clicked"));
  QVERIFY2(ok->isEnabled(), "the broken profile is gone");
  QCOMPARE(list->count(), 1);
  QCOMPARE(dialog.getProfiles().keys(), QStringList{QStringLiteral("one")});
}

/**
 * @brief Renaming the active profile keeps it active under its new name;
 * deleting it clears the active profile instead of leaving a name behind
 * that no longer exists. A rename is not a new store, so no initialisation
 * prompt appears for it.
 */
void tst_configdialog::activeProfileFollowsRenameAndDeletion() {
  struct ProfileRestorer {
    Profiles saved;
    AppSettings settings;
    ~ProfileRestorer() {
      QtPassSettings::setProfiles(saved);
      QtPassSettings::save(settings);
    }
  } restorer{QtPassSettings::getProfiles(), QtPassSettings::load()};

  QTemporaryDir work;
  QTemporaryDir home;
  for (const QTemporaryDir *dir : {&work, &home}) {
    QFile gpgId(QDir(dir->path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("0000000000000000\n");
  }
  Profiles profiles;
  Profile w;
  w.path = work.path();
  profiles.insert(QStringLiteral("work"), w);
  Profile h;
  h.path = home.path();
  profiles.insert(QStringLiteral("home"), h);
  QtPassSettings::setProfiles(profiles);
  {
    AppSettings s = QtPassSettings::load();
    s.activeProfile = QStringLiteral("work");
    QtPassSettings::save(s);
  }

  {
    ConfigDialog dialog(nullptr);
    auto *name = child<QLineEdit>(dialog, "profileName");
    QCOMPARE(name->text(), QStringLiteral("work"));
    name->setText(QStringLiteral("office"));
    emit name->textEdited(name->text());
    QVERIFY(QMetaObject::invokeMethod(&dialog, "on_accepted"));
  }
  QCOMPARE(QtPassSettings::getProfile(), QStringLiteral("office"));
  QCOMPARE(QtPassSettings::getProfiles().keys(),
           (QStringList{QStringLiteral("home"), QStringLiteral("office")}));

  {
    ConfigDialog dialog(nullptr);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("profileList"));
    QCOMPARE(list->currentItem()->text(), QStringLiteral("office"));
    QVERIFY(QMetaObject::invokeMethod(&dialog, "on_deleteButton_clicked"));
    QVERIFY(QMetaObject::invokeMethod(&dialog, "on_accepted"));
  }
  QVERIFY2(QtPassSettings::getProfile().isEmpty(),
           qPrintable(QtPassSettings::getProfile()));
  QCOMPARE(QtPassSettings::getProfiles().keys(),
           QStringList{QStringLiteral("home")});
}

/**
 * @brief Every label that names a field points at it, so screen readers
 *        announce the field by that name and Alt+mnemonic could focus it.
 */
void tst_configdialog::fieldLabelsHaveBuddies() {
  ConfigDialog dialog(nullptr);
  const QList<QPair<QString, QString>> pairs = {
      {QStringLiteral("labelGpgPath"), QStringLiteral("gpgPath")},
      {QStringLiteral("labelGitPath"), QStringLiteral("gitPath")},
      {QStringLiteral("labelPwgenPath"), QStringLiteral("pwgenPath")},
      {QStringLiteral("labelPassPath"), QStringLiteral("passPath")},
      {QStringLiteral("labelSshAuthSock"),
       QStringLiteral("sshAuthSockOverride")},
      {QStringLiteral("labelStorePath"), QStringLiteral("storePath")},
      {QStringLiteral("labelProfileName"), QStringLiteral("profileName")},
      {QStringLiteral("labelProfilePath"), QStringLiteral("profilePath")},
      {QStringLiteral("labelProfileSigningKey"),
       QStringLiteral("profileSigningKey")},
      {QStringLiteral("label_7"), QStringLiteral("spinBoxPasswordLength")},
      {QStringLiteral("labelPasswordChars"),
       QStringLiteral("passwordCharTemplateSelector")},
      {QStringLiteral("labelSeconds"),
       QStringLiteral("spinBoxAutoclearSeconds")},
      {QStringLiteral("labelPanelSeconds"),
       QStringLiteral("spinBoxAutoclearPanelSeconds")},
      {QStringLiteral("labelLength"), QStringLiteral("spinBoxPasswordLength")},
  };
  for (const auto &[labelName, fieldName] : pairs) {
    auto *label = dialog.findChild<QLabel *>(labelName);
    QVERIFY2(label != nullptr, qPrintable(labelName + " must exist"));
    QVERIFY2(label->buddy() != nullptr,
             qPrintable(labelName + " must have a buddy"));
    QCOMPARE(label->buddy()->objectName(), fieldName);
  }
}

/**
 * @brief The "…" browse buttons carry no text a screen reader can use; they
 *        need an accessible name and a tooltip.
 */
void tst_configdialog::browseButtonsAreNamed() {
  ConfigDialog dialog(nullptr);
  for (const QString &name :
       {QStringLiteral("toolButtonGpg"), QStringLiteral("toolButtonGit"),
        QStringLiteral("toolButtonPwgen"), QStringLiteral("toolButtonPass"),
        QStringLiteral("toolButtonStore")}) {
    auto *button = dialog.findChild<QToolButton *>(name);
    QVERIFY2(button != nullptr, qPrintable(name + " must exist"));
    QVERIFY2(!button->accessibleName().isEmpty(),
             qPrintable(name + " must have an accessible name"));
    QVERIFY2(!button->toolTip().isEmpty(),
             qPrintable(name + " must have a tooltip"));
  }
}

/**
 * @brief Both autoclear spin boxes open on "Never" and must show all of it;
 *        the layout used to size them for "0" and cut it to "ver".
 */
void tst_configdialog::autoclearBoxesAreWideEnoughForNever() {
  SettingsRestorer restorer;
  AppSettings seed = QtPassSettings::load();
  seed.useAutoclear = false;
  seed.useAutoclearPanel = false;
  QtPassSettings::save(seed);
  ConfigDialog dialog(nullptr);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  auto *pages = child<QStackedWidget>(dialog, "pages");
  for (const char *name :
       {"spinBoxAutoclearSeconds", "spinBoxAutoclearPanelSeconds"}) {
    auto *box = autoclearBox(dialog, name);
    QVERIFY(box != nullptr);
    QCOMPARE(box->text(), QStringLiteral("Never"));
    for (int i = 0; i < pages->count(); ++i) {
      if (pages->widget(i)->isAncestorOf(box)) {
        pages->setCurrentIndex(i);
      }
    }
    QTRY_VERIFY2(box->width() >= box->sizeHint().width(),
                 qPrintable(QStringLiteral("%1 is %2 wide, needs %3")
                                .arg(QLatin1String(name))
                                .arg(box->width())
                                .arg(box->sizeHint().width())));
  }
}

/**
 * @brief The dialog used to open at its layout minimum (659x728) with no
 *        scroll area, so a long translation pushed OK off a 1280x720 screen.
 *        Every tab now scrolls; the dialog itself has to be small enough for
 *        a 1366x768 laptop with room to spare.
 */
void tst_configdialog::dialogCanShrinkBelowItsOldMinimum() {
  ConfigDialog dialog(nullptr);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  const QSize minimum = dialog.minimumSizeHint();
  QVERIFY2(minimum.height() < 400,
           qPrintable(QStringLiteral("minimum height %1 must leave room on a "
                                     "small screen")
                          .arg(minimum.height())));
  QVERIFY2(minimum.width() < 500,
           qPrintable(QStringLiteral("minimum width %1 must allow a narrow "
                                     "dialog")
                          .arg(minimum.width())));
}

/**
 * @brief The six bold labels that only pretended to be section headers are
 *        real QGroupBoxes now, titled like the other group boxes: no colon.
 */
void tst_configdialog::sectionHeadersAreGroupBoxes() {
  ConfigDialog dialog(nullptr);
  const QList<QPair<QString, QString>> groups = {
      {QStringLiteral("groupBoxClipboard"), QStringLiteral("Clipboard")},
      {QStringLiteral("groupBoxContentPanel"), QStringLiteral("Content panel")},
      {QStringLiteral("groupBoxPasswordGeneration"),
       QStringLiteral("Password generation")},
      {QStringLiteral("groupBoxGit"), QStringLiteral("Git")},
      {QStringLiteral("groupBoxExtensions"), QStringLiteral("Extensions")},
      {QStringLiteral("groupBoxSystem"), QStringLiteral("System")},
  };
  for (const auto &[name, title] : groups) {
    auto *box = dialog.findChild<QGroupBox *>(name);
    QVERIFY2(box != nullptr, qPrintable(name + " must exist"));
    QCOMPARE(box->title(), title);
  }
  QVERIFY2(dialog.findChild<QLabel *>(QStringLiteral("label_10")) == nullptr,
           "the pseudo-header labels must be gone");
}

/**
 * @brief pass matches PASSWORD_STORE_SIGNING_KEY against VALIDSIG
 *        fingerprints, so a short key ID would sign and never verify; the
 *        form refuses anything but full fingerprints.
 */
void tst_configdialog::signingKeyMustBeAFullFingerprint() {
  struct ProfileRestorer {
    Profiles saved;
    QString active;
    ~ProfileRestorer() {
      QtPassSettings::setProfiles(saved);
      AppSettings s = QtPassSettings::load();
      s.activeProfile = active;
      QtPassSettings::save(s);
    }
  } restorer{QtPassSettings::getProfiles(),
             QtPassSettings::load().activeProfile};
  Profiles profiles;
  Profile one;
  one.path = QStringLiteral("/store/one");
  profiles.insert(QStringLiteral("one"), one);
  QtPassSettings::setProfiles(profiles);
  {
    AppSettings s = QtPassSettings::load();
    s.activeProfile = QStringLiteral("one");
    QtPassSettings::save(s);
  }

  ConfigDialog dialog(nullptr);
  auto *key = child<QLineEdit>(dialog, "profileSigningKey");
  auto *ok = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))
                 ->button(QDialogButtonBox::Ok);
  QVERIFY(key != nullptr && ok != nullptr);
  const QString tip = key->toolTip();
  QVERIFY(ok->isEnabled());

  key->setText(QStringLiteral("0123456789ABCDEF"));
  emit key->textEdited(key->text());
  QVERIFY2(!ok->isEnabled(), "a long key ID is not a fingerprint");
  QVERIFY(key->toolTip().contains(QStringLiteral("40 or 64")));

  key->setText(QStringLiteral("0123456789abcdef0123456789abcdef01234567 "
                              "FEDCBA9876543210FEDCBA9876543210FEDCBA98"));
  emit key->textEdited(key->text());
  QVERIFY2(ok->isEnabled(), "two fingerprints, either case, are fine");
  QCOMPARE(key->toolTip(), tip);

  key->setText(QString());
  emit key->textEdited(key->text());
  QVERIFY2(ok->isEnabled(), "no signing key is a valid choice");
  QVERIFY(ConfigDialog::isFingerprintList(QString()));
  QVERIFY(
      !ConfigDialog::isFingerprintList(QStringLiteral("alice@example.org")));
  QVERIFY(ConfigDialog::isFingerprintList(QString(64, QLatin1Char('a'))));
}

/**
 * @brief The dialog is a sidebar of pages: one list entry per stacked page,
 *        every old group box on one of them, and the list drives the stack.
 */
void tst_configdialog::pagesFollowTheSidebar() {
  ConfigDialog dialog(nullptr);
  auto *list = child<QListWidget>(dialog, "pageList");
  auto *pages = child<QStackedWidget>(dialog, "pages");
  QCOMPARE(list->count(), pages->count());
  QCOMPARE(list->count(), 7);
  list->setCurrentRow(0);
  QCOMPARE(pages->currentIndex(), 0);
  list->setCurrentRow(3);
  QCOMPARE(pages->currentIndex(), 3);
  const QList<QPair<QString, QString>> homes = {
      {QStringLiteral("groupBoxSystem"), QStringLiteral("pageGeneral")},
      {QStringLiteral("groupBoxExtensions"), QStringLiteral("pageGeneral")},
      {QStringLiteral("groupBoxClipboard"), QStringLiteral("pageClipboard")},
      {QStringLiteral("groupBoxContentPanel"), QStringLiteral("pageDisplay")},
      {QStringLiteral("groupBoxPasswordGeneration"),
       QStringLiteral("pagePasswords")},
      {QStringLiteral("groupBoxTemplate"), QStringLiteral("pagePasswords")},
      {QStringLiteral("groupBoxGit"), QStringLiteral("pageGit")},
      {QStringLiteral("groupBoxNative"), QStringLiteral("pagePrograms")},
      {QStringLiteral("profileForm"), QStringLiteral("pageProfiles")},
  };
  for (const auto &[box, page] : homes) {
    auto *widget = dialog.findChild<QGroupBox *>(box);
    QVERIFY2(widget != nullptr, qPrintable(box + " must exist"));
    QVERIFY2(child<QWidget>(dialog, page.toLatin1().constData())
                 ->isAncestorOf(widget),
             qPrintable(box + " must live on " + page));
  }
}

/**
 * @brief A configuration that cannot work (no programs found) opens on the
 *        Programs page, where the fix is.
 */
void tst_configdialog::invalidConfigurationOpensPrograms() {
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable.clear();
    s.gpgExecutable.clear();
    s.gitExecutable.clear();
    s.usePass = false;
    QtPassSettings::save(s);
  }
  ConfigDialog dialog(nullptr);
  auto *pages = child<QStackedWidget>(dialog, "pages");
  QCOMPARE(pages->currentWidget()->objectName(),
           QStringLiteral("pagePrograms"));
  QCOMPARE(child<QListWidget>(dialog, "pageList")->currentRow(),
           pages->currentIndex());
}

/**
 * @brief The regression net for #1602's six config-clobbering paths: flip
 *        every setting the dialog owns to a non-default value, accept, load
 *        again, and expect exactly those values back. Keys the dialog does
 *        not own (window geometry, profiles, ...) must survive untouched.
 */
void tst_configdialog::acceptRoundTripsEveryOwnedSetting() {
  {
    AppSettings seed = QtPassSettings::load();
    seed.version = QStringLiteral("0.0.0-seed");
    QtPassSettings::save(seed);
    QtPassSettings::setGeometry(QByteArrayLiteral("keep-me"));
  }
  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "passPath")->setText(QStringLiteral("/opt/pass"));
  child<QLineEdit>(dialog, "gitPath")->setText(QStringLiteral("/opt/git"));
  child<QLineEdit>(dialog, "gpgPath")->setText(QStringLiteral("/opt/gpg"));
  child<QLineEdit>(dialog, "pwgenPath")->setText(QStringLiteral("/opt/pwgen"));
  child<QLineEdit>(dialog, "storePath")->setText(QStringLiteral("/tmp/store"));
  child<QLineEdit>(dialog, "sshAuthSockOverride")->setText(QString());
  child<QComboBox>(dialog, "comboBoxClipboard")->setCurrentIndex(2);
  child<QSpinBox>(dialog, "spinBoxAutoclearSeconds")->setValue(42);
  child<QSpinBox>(dialog, "spinBoxAutoclearPanelSeconds")->setValue(43);
  child<QSpinBox>(dialog, "spinBoxPasswordLength")->setValue(31);
  child<QPlainTextEdit>(dialog, "plainTextEditTemplate")
      ->setPlainText(QStringLiteral("login\nurl"));
  for (const char *box :
       {"checkBoxHidePassword", "checkBoxHideContent", "checkBoxUseMonospace",
        "checkBoxDisplayAsIs", "checkBoxNoLineWrapping", "checkBoxAddGPGId",
        "checkBoxUseGit", "checkBoxUseOtp", "checkBoxUseGrepSearch",
        "checkBoxUsePwgen", "checkBoxAvoidCapitals", "checkBoxAvoidNumbers",
        "checkBoxLessRandom", "checkBoxUseSymbols"}) {
    child<QCheckBox>(dialog, box)->setChecked(true);
  }
  child<QComboBox>(dialog, "comboBoxFields")->setCurrentIndex(2);
  dialog.accept();

  const AppSettings s = QtPassSettings::load();
  QCOMPARE(s.passExecutable, QStringLiteral("/opt/pass"));
  QCOMPARE(s.gitExecutable, QStringLiteral("/opt/git"));
  QCOMPARE(s.gpgExecutable, QStringLiteral("/opt/gpg"));
  QCOMPARE(s.pwgenExecutable, QStringLiteral("/opt/pwgen"));
  QCOMPARE(s.passStore, QStringLiteral("/tmp/store/"));
  QCOMPARE(s.clipBoardType, Enums::CLIPBOARD_ON_DEMAND);
  QCOMPARE(s.autoclearSeconds, 42);
  QCOMPARE(s.autoclearPanelSeconds, 43);
  QCOMPARE(s.passwordConfiguration.length, 31);
  QCOMPARE(s.passTemplate, QStringLiteral("login\nurl"));
  for (bool value :
       {s.useAutoclear, s.useAutoclearPanel, s.hidePassword, s.hideContent,
        s.useMonospace, s.displayAsIs, s.noLineWrapping, s.addGPGId, s.useGit,
        s.useOtp, s.useGrepSearch, s.usePwgen, s.avoidCapitals, s.avoidNumbers,
        s.lessRandom, s.useSymbols, s.useTemplate, s.templateAllFields}) {
    QVERIFY(value);
  }
  QCOMPARE(s.version, QStringLiteral(VERSION));
  QCOMPARE(QtPassSettings::getGeometry(), QByteArrayLiteral("keep-me"));
}

/**
 * @brief Since #1140 the dialog stored auto push/pull per profile and no
 *        longer wrote the global keys the backends read, so both checkboxes
 *        had no effect.
 */
void tst_configdialog::acceptSavesTheGlobalAutoPushAndAutoPull() {
  {
    AppSettings seed = QtPassSettings::load();
    seed.useGit = false;
    seed.autoPush = false;
    seed.autoPull = false;
    QtPassSettings::save(seed);
  }
  ConfigDialog dialog(nullptr);
  child<QCheckBox>(dialog, "checkBoxUseGit")->setChecked(true);
  child<QCheckBox>(dialog, "checkBoxAutoPush")->setChecked(true);
  child<QCheckBox>(dialog, "checkBoxAutoPull")->setChecked(true);
  dialog.accept();
  const AppSettings s = QtPassSettings::load();
  QVERIFY(s.useGit);
  QVERIFY2(s.autoPush, "auto push must reach the global setting");
  QVERIFY2(s.autoPull, "auto pull must reach the global setting");
  QVERIFY(QtPassSettings::isAutoPush());
}

/**
 * @brief With no qrencode on PATH and none configured, the QR checkbox is
 *        disabled and says why, and the stored path is left empty rather
 *        than being filled with a guess.
 */
void tst_configdialog::qrencodeMissingDisablesTheCheckbox() {
  SettingsRestorer restorer;
  {
    AppSettings s = QtPassSettings::load();
    s.qrencodeExecutable.clear();
    QtPassSettings::save(s);
  }
  ConfigDialog dialog(nullptr);
  auto *box = child<QCheckBox>(dialog, "checkBoxUseQrencode");
  QVERIFY2(!box->isEnabled(), "no qrencode anywhere must disable the box");
  QVERIFY2(box->toolTip().contains(QStringLiteral("qrencode needs")),
           qPrintable(box->toolTip()));
  QVERIFY2(QtPassSettings::load().qrencodeExecutable.isEmpty(),
           "a miss must not store a path");
}

/**
 * @brief A configured qrencode path that names an executable file is taken
 *        as is: the checkbox is usable, PATH is not consulted and the stored
 *        path is left alone (no qrencode is on the narrowed PATH here).
 */
void tst_configdialog::qrencodeConfiguredExecutableEnablesTheCheckbox() {
#ifdef Q_OS_WIN
  QSKIP("qrencode is never available on Windows");
#endif
  SettingsRestorer restorer;
  {
    AppSettings s = QtPassSettings::load();
    // Any executable file will do for the probe; the stand-in gpg is one.
    s.qrencodeExecutable = m_gpg;
    QtPassSettings::save(s);
  }
  ConfigDialog dialog(nullptr);
  auto *box = child<QCheckBox>(dialog, "checkBoxUseQrencode");
  QVERIFY2(box->isEnabled(),
           "a configured executable qrencode must enable the box");
  QVERIFY2(!box->toolTip().contains(QStringLiteral("qrencode needs")),
           qPrintable(box->toolTip()));
  QCOMPARE(QtPassSettings::load().qrencodeExecutable, m_gpg);
}

/**
 * @brief With no usable configured path, a qrencode found on PATH is stored
 *        so the QR display can run it, and the checkbox is usable.
 */
void tst_configdialog::qrencodeOnPathIsStored() {
#ifdef Q_OS_WIN
  QSKIP("qrencode is never available on Windows");
#endif
  SettingsRestorer restorer;
  {
    AppSettings s = QtPassSettings::load();
    // A path that is not a file: the configured value must be ignored.
    s.qrencodeExecutable = m_bin.path();
    QtPassSettings::save(s);
  }
  const QString qrencode =
      QDir(m_bin.path()).filePath(QStringLiteral("qrencode"));
  QVERIFY2(writeScript(qrencode, "exit 0\n"),
           qPrintable("cannot write " + qrencode));
  const auto removeStandIn =
      qScopeGuard([&qrencode]() { QFile::remove(qrencode); });

  ConfigDialog dialog(nullptr);
  auto *box = child<QCheckBox>(dialog, "checkBoxUseQrencode");
  QVERIFY2(box->isEnabled(), "qrencode on PATH must enable the box");
  QCOMPARE(
      QFileInfo(QtPassSettings::load().qrencodeExecutable).canonicalFilePath(),
      QFileInfo(qrencode).canonicalFilePath());
}

/**
 * @brief Clicking the tray checkbox gates "hide on close" and "start
 *        minimized": both only make sense with a tray icon to come back
 *        from. Offscreen has no tray, so the constructor disables the box;
 *        the test re-enables it to drive the click as a desktop user would.
 */
void tst_configdialog::trayIconCheckboxGatesItsCompanions() {
  ConfigDialog dialog(nullptr);
  auto *tray = child<QCheckBox>(dialog, "checkBoxUseTrayIcon");
  auto *hideOnClose = child<QCheckBox>(dialog, "checkBoxHideOnClose");
  auto *startMinimized = child<QCheckBox>(dialog, "checkBoxStartMinimized");
  tray->setEnabled(true);
  tray->setChecked(false);
  hideOnClose->setEnabled(false);
  startMinimized->setEnabled(false);

  tray->click();
  QVERIFY2(tray->isChecked(), "the click ticks the box");
  QVERIFY2(hideOnClose->isEnabled(),
           "a tray icon makes hide-on-close selectable");
  QVERIFY2(startMinimized->isEnabled(),
           "a tray icon makes start-minimized selectable");

  tray->click();
  QVERIFY2(!tray->isChecked(), "the second click clears the box");
  QVERIFY2(!hideOnClose->isEnabled(),
           "no tray icon, nowhere to hide to on close");
  QVERIFY2(!startMinimized->isEnabled(),
           "no tray icon, nowhere to start minimized into");
}

/**
 * @brief Autodetect fills every program field from PATH, falls back from
 *        gpg2 to gpg, and switches to pass mode when pass is found.
 */
void tst_configdialog::autodetectFindsTheProgramsOnPath() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in programs are shell scripts");
#endif
  ConfigDialog dialog(nullptr);
  for (const char *field : {"passPath", "gpgPath", "gitPath", "pwgenPath"}) {
    child<QLineEdit>(dialog, field)->clear();
  }
  child<QRadioButton>(dialog, "radioButtonNative")->click();
  QVERIFY2(!child<QRadioButton>(dialog, "radioButtonPass")->isChecked(),
           "precondition: native mode before autodetect");

  child<QToolButton>(dialog, "autodetectButton")->click();

  const auto canonical = [](const QString &path) {
    return QFileInfo(path).canonicalFilePath();
  };
  QCOMPARE(canonical(child<QLineEdit>(dialog, "passPath")->text()),
           canonical(m_pass));
  QCOMPARE(canonical(child<QLineEdit>(dialog, "gpgPath")->text()),
           canonical(m_gpg));
  QCOMPARE(canonical(child<QLineEdit>(dialog, "gitPath")->text()),
           canonical(m_git));
  QCOMPARE(canonical(child<QLineEdit>(dialog, "pwgenPath")->text()),
           canonical(m_pwgen));
  QVERIFY2(child<QRadioButton>(dialog, "radioButtonPass")->isChecked(),
           "finding pass selects pass mode");
  QVERIFY2(child<QGroupBox>(dialog, "groupBoxPass")->isEnabled(),
           "pass mode enables the pass group box");
}

/**
 * @brief The backend radio buttons enable their own group box and, in pass
 *        mode, switch off every password-generation control (pass generates
 *        itself); native mode gives them back, pwgen only with a path.
 */
void tst_configdialog::backendRadioButtonsToggleTheGroupBoxes() {
  ConfigDialog dialog(nullptr);
  auto *pwgenPath = child<QLineEdit>(dialog, "pwgenPath");
  pwgenPath->setText(QStringLiteral("/opt/pwgen"));

  child<QRadioButton>(dialog, "radioButtonPass")->click();
  QVERIFY2(child<QGroupBox>(dialog, "groupBoxPass")->isEnabled(),
           "pass mode enables the pass group box");
  QVERIFY2(!child<QGroupBox>(dialog, "groupBoxNative")->isEnabled(),
           "pass mode disables the native group box");
  for (const char *control :
       {"spinBoxPasswordLength", "checkBoxUsePwgen", "checkBoxAvoidCapitals",
        "checkBoxUseSymbols", "checkBoxLessRandom", "checkBoxAvoidNumbers",
        "passwordCharTemplateSelector", "lineEditPasswordChars",
        "labelPasswordChars"}) {
    QVERIFY2(!child<QWidget>(dialog, control)->isEnabled(),
             qPrintable(QLatin1String(control) + " must be off in pass mode"));
  }

  child<QRadioButton>(dialog, "radioButtonNative")->click();
  QVERIFY2(!child<QGroupBox>(dialog, "groupBoxPass")->isEnabled(),
           "native mode disables the pass group box");
  QVERIFY2(child<QGroupBox>(dialog, "groupBoxNative")->isEnabled(),
           "native mode enables the native group box");
  QVERIFY2(child<QSpinBox>(dialog, "spinBoxPasswordLength")->isEnabled(),
           "native mode gives the length spinner back");
  QVERIFY2(child<QCheckBox>(dialog, "checkBoxUsePwgen")->isEnabled(),
           "a pwgen path makes pwgen selectable again");

  pwgenPath->clear();
  child<QRadioButton>(dialog, "radioButtonNative")->click();
  QVERIFY2(!child<QCheckBox>(dialog, "checkBoxUsePwgen")->isEnabled(),
           "without a pwgen path the box stays off in native mode");
}

/**
 * @brief Picking a character set in the selector shows that set's saved
 *        characters and only lets the custom one be edited.
 */
void tst_configdialog::charsetSelectorFollowsTheSavedSets() {
  SettingsRestorer restorer;
  {
    AppSettings s = QtPassSettings::load();
    s.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM] =
        QStringLiteral("xyz789");
    QtPassSettings::save(s);
  }
  ConfigDialog dialog(nullptr);
  auto *selector = child<QComboBox>(dialog, "passwordCharTemplateSelector");
  auto *chars = child<QLineEdit>(dialog, "lineEditPasswordChars");

  emit selector->activated(PasswordConfiguration::ALPHANUMERIC);
  QCOMPARE(
      chars->text(),
      PasswordConfiguration().Characters[PasswordConfiguration::ALPHANUMERIC]);
  QVERIFY2(!chars->isEnabled(), "a builtin set is read-only");

  emit selector->activated(PasswordConfiguration::CUSTOM);
  QCOMPARE(chars->text(), QStringLiteral("xyz789"));
  QVERIFY2(chars->isEnabled(), "the custom set is editable");
}

/**
 * @brief Click a browse button and answer the file dialog it opens: with a
 *        path, select it and accept; with none, cancel.
 * @return empty when exactly one dialog came by and closed as driven; else
 *         what went wrong (a QFileDialog that refused the name shows up as
 *         its "file not found" QMessageBox plus the dialog left stuck).
 */
auto tst_configdialog::browse(ConfigDialog &dialog, const char *button,
                              const QString &choose) -> QString {
  ModalDriver driver([&choose](QWidget *modal) {
    if (auto *fileDialog = qobject_cast<QFileDialog *>(modal)) {
      if (choose.isEmpty()) {
        fileDialog->reject();
        return;
      }
      if (fileDialog->fileMode() == QFileDialog::Directory) {
        fileDialog->setDirectory(choose);
      } else {
        // selectFile() leaves the name edit alone while it has focus (a
        // user is typing); drop focus first so the selection lands.
        if (QWidget *focused = fileDialog->focusWidget()) {
          focused->clearFocus();
        }
        fileDialog->selectFile(choose);
      }
      // QFileDialog redeclares accept() protected; the QDialog view of it
      // is public and still dispatches virtually to the QFileDialog logic.
      // Run it from a posted event rather than inside the driver's timer
      // slot: a nested exec() started from a timer activation (QFileDialog's
      // own "file not found" complaint) no longer receives that timer's
      // timeouts, so the driver could neither answer it nor unstick the
      // dialog.
      modal->setProperty("tst_pending", true);
      QMetaObject::invokeMethod(
          fileDialog,
          [fileDialog]() {
            fileDialog->setProperty("tst_pending", false);
            static_cast<QDialog *>(fileDialog)->accept();
          },
          Qt::QueuedConnection);
      return;
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  child<QToolButton>(dialog, button)->click();
  if (!driver.stuck().isEmpty()) {
    return QStringLiteral("%1: dialog(s) did not close when driven: %2")
        .arg(QLatin1String(button), driver.stuck().join(QLatin1String(", ")));
  }
  if (driver.seen() != 1) {
    return QStringLiteral("%1: expected exactly one dialog, saw %2")
        .arg(QLatin1String(button))
        .arg(driver.seen());
  }
  return {};
}

/**
 * @brief The four program browse buttons put the chosen file into their
 *        field; git and pwgen also switch their "use" checkbox back on.
 */
void tst_configdialog::browseButtonsTakeTheChosenFile() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in programs are shell scripts");
#endif
  ConfigDialog dialog(nullptr);
  const QList<std::tuple<const char *, const char *, QString>> picks = {
      {"toolButtonGit", "gitPath", m_git},
      {"toolButtonGpg", "gpgPath", m_gpg},
      {"toolButtonPass", "passPath", m_pass},
      {"toolButtonPwgen", "pwgenPath", m_pwgen},
  };
  child<QCheckBox>(dialog, "checkBoxUseGit")->setEnabled(false);
  child<QCheckBox>(dialog, "checkBoxUsePwgen")->setEnabled(false);
  for (const auto &[button, field, path] : picks) {
    // Each browse button lives in the group box of its backend, and only the
    // selected backend's group is enabled; a disabled button ignores click().
    const bool passButton = qstrcmp(button, "toolButtonPass") == 0;
    child<QRadioButton>(dialog,
                        passButton ? "radioButtonPass" : "radioButtonNative")
        ->click();
    QVERIFY2(child<QToolButton>(dialog, button)->isEnabled(), button);
    child<QLineEdit>(dialog, field)->clear();
    const QString problem = browse(dialog, button, path);
    QVERIFY2(problem.isEmpty(), qPrintable(problem));
    QCOMPARE(child<QLineEdit>(dialog, field)->text(), path);
  }
  QVERIFY2(child<QCheckBox>(dialog, "checkBoxUseGit")->isEnabled(),
           "a git binary makes git selectable");
  QVERIFY2(child<QCheckBox>(dialog, "checkBoxUsePwgen")->isEnabled(),
           "a pwgen binary makes pwgen selectable");
}

/**
 * @brief Cancelling a program browse leaves gpg and pass alone, but git and
 *        pwgen are switched off: their file dialog is the only way to say
 *        "none" and the checkbox must not promise a tool that is not there.
 */
void tst_configdialog::browseButtonsLeaveTheFieldWhenCancelled() {
  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "gpgPath")->setText(QStringLiteral("/opt/gpg"));
  child<QLineEdit>(dialog, "passPath")->setText(QStringLiteral("/opt/pass"));
  // The pass button sits in the pass group box, enabled only in pass mode.
  child<QRadioButton>(dialog, "radioButtonPass")->click();
  QString problem = browse(dialog, "toolButtonPass", QString());
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  child<QRadioButton>(dialog, "radioButtonNative")->click();
  problem = browse(dialog, "toolButtonGpg", QString());
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QCOMPARE(child<QLineEdit>(dialog, "gpgPath")->text(),
           QStringLiteral("/opt/gpg"));
  QCOMPARE(child<QLineEdit>(dialog, "passPath")->text(),
           QStringLiteral("/opt/pass"));

  auto *useGit = child<QCheckBox>(dialog, "checkBoxUseGit");
  useGit->setEnabled(true);
  useGit->setChecked(true);
  problem = browse(dialog, "toolButtonGit", QString());
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QVERIFY2(!useGit->isChecked(), "no git binary means no git");
  QVERIFY2(!useGit->isEnabled(), "and none to select");
  QVERIFY2(!child<QCheckBox>(dialog, "checkBoxAutoPush")->isEnabled(),
           "no git means nothing to push with");

  auto *usePwgen = child<QCheckBox>(dialog, "checkBoxUsePwgen");
  usePwgen->setEnabled(true);
  usePwgen->setChecked(true);
  problem = browse(dialog, "toolButtonPwgen", QString());
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QVERIFY2(!usePwgen->isChecked(), "no pwgen binary means no pwgen");
  QVERIFY2(!usePwgen->isEnabled(), "and none to select");
}

/**
 * @brief The store and profile-path browse buttons take a folder; the
 *        profile one also writes it into the selected profile.
 */
void tst_configdialog::folderBrowseButtonsTakeTheChosenFolder() {
  SettingsRestorer restorer;
  Profiles profiles;
  Profile one;
  one.path = QStringLiteral("/store/one");
  profiles.insert(QStringLiteral("one"), one);
  QtPassSettings::setProfiles(profiles);

  QTemporaryDir folder;
  QVERIFY2(folder.isValid(), qPrintable(folder.errorString()));
  const QString chosen = QDir(folder.path()).canonicalPath();

  ConfigDialog dialog(nullptr);
  QString problem = browse(dialog, "toolButtonStore", chosen);
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QCOMPARE(QDir(child<QLineEdit>(dialog, "storePath")->text()).canonicalPath(),
           chosen);

  child<QListWidget>(dialog, "profileList")->setCurrentRow(0);
  problem = browse(dialog, "profilePathBrowse", chosen);
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QCOMPARE(
      QDir(child<QLineEdit>(dialog, "profilePath")->text()).canonicalPath(),
      chosen);
  QCOMPARE(QDir(dialog.getProfiles().value(QStringLiteral("one")).path)
               .canonicalPath(),
           chosen);
}

/**
 * @brief Cancelling a folder browse keeps what was in the field.
 */
void tst_configdialog::folderBrowseCancelLeavesTheField() {
  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "storePath")->setText(QStringLiteral("/keep/me"));
  const QString problem = browse(dialog, "toolButtonStore", QString());
  QVERIFY2(problem.isEmpty(), qPrintable(problem));
  QCOMPARE(child<QLineEdit>(dialog, "storePath")->text(),
           QStringLiteral("/keep/me"));
}

/**
 * @brief "Generate key" opens the keygen dialog against the gpg named in the
 *        form (its template follows what that gpg reports), not against
 *        whatever is saved.
 */
void tst_configdialog::generateKeyOpensTheKeygenDialog() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in gpg is a shell script");
#endif
  SettingsRestorer restorer;
  QTemporaryDir store;
  QVERIFY(store.isValid());
  {
    AppSettings s = QtPassSettings::load();
    s.passStore = store.path() + QLatin1Char('/');
    s.usePass = false;
    s.gpgExecutable = m_gpg;
    QtPassSettings::save(s);
  }
  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "gpgPath")->setText(m_gpg);

  QString className;
  QString keyTemplate;
  ModalDriver driver([&](QWidget *modal) {
    className = QLatin1String(modal->metaObject()->className());
    if (auto *edit = modal->findChild<QPlainTextEdit *>(
            QStringLiteral("plainTextEdit"))) {
      keyTemplate = edit->toPlainText();
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  child<QPushButton>(dialog, "pushButtonGenerateKey")->click();

  QCOMPARE(driver.seen(), 1);
  QCOMPARE(className, QStringLiteral("KeygenDialog"));
  QVERIFY2(
      keyTemplate.contains(QStringLiteral("Ed25519")),
      qPrintable("gpg 2.4 gets the Ed25519 template, got: " + keyTemplate));
}

/**
 * @brief Delete with nothing selected explains itself instead of failing
 *        silently, and changes nothing.
 */
void tst_configdialog::deleteWithoutASelectionWarns() {
  SettingsRestorer restorer;
  QtPassSettings::setProfiles(Profiles());
  ConfigDialog dialog(nullptr);
  QCOMPARE(child<QListWidget>(dialog, "profileList")->count(), 0);

  QString title;
  QString text;
  ModalDriver driver([&title, &text](QWidget *modal) {
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      title = box->windowTitle();
      text = box->text();
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  QVERIFY(QMetaObject::invokeMethod(&dialog, "on_deleteButton_clicked"));

  QCOMPARE(driver.seen(), 1);
  COMPARE_BOX_TITLE(title, QStringLiteral("No profile selected"));
  QVERIFY2(text.contains(QStringLiteral("No profile selected to delete")),
           qPrintable(text));
  QVERIFY2(dialog.getProfiles().isEmpty(), "nothing to delete, nothing gone");
}

namespace {
/**
 * @brief Accept the dialog and return the text of the one message box that
 *        comes by (empty when none did).
 */
auto acceptAndCollectWarning(ConfigDialog &dialog, QString *title) -> QString {
  QString text;
  ModalDriver driver([&](QWidget *modal) {
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      *title = box->windowTitle();
      text = box->text();
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  dialog.accept();
  return driver.seen() == 1 ? text
                            : QStringLiteral("<%1 dialogs>").arg(driver.seen());
}
} // namespace

/**
 * @brief An SSH_AUTH_SOCK override that points nowhere is saved as typed,
 *        but the user is told the path does not exist.
 */
void tst_configdialog::sshAuthSockOverrideWarnsWhenMissing() {
  SettingsRestorer restorer;
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString missing = QDir(dir.path()).filePath(QStringLiteral("gone"));

  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "sshAuthSockOverride")->setText(missing);
  QString title;
  const QString text = acceptAndCollectWarning(dialog, &title);

  COMPARE_BOX_TITLE(
      title, QStringLiteral("Potentially invalid SSH_AUTH_SOCK override"));
  QVERIFY2(text.contains(QStringLiteral("does not exist")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("still be saved")), qPrintable(text));
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, missing);
}

/**
 * @brief A regular file is not an agent socket; the warning says so and the
 *        value is still saved.
 */
void tst_configdialog::sshAuthSockOverrideWarnsForARegularFile() {
#ifdef Q_OS_WIN
  QSKIP("only Unix can tell a socket from a file");
#endif
  SettingsRestorer restorer;
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral("plain"));
  {
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }

  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "sshAuthSockOverride")->setText(file);
  QString title;
  const QString text = acceptAndCollectWarning(dialog, &title);

  COMPARE_BOX_TITLE(
      title, QStringLiteral("Potentially invalid SSH_AUTH_SOCK override"));
  QVERIFY2(text.contains(QStringLiteral("not a Unix domain socket")),
           qPrintable(text));
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, file);
}

/**
 * @brief A path we cannot read is reported as such before the socket check.
 */
void tst_configdialog::sshAuthSockOverrideWarnsWhenUnreadable() {
#ifdef Q_OS_WIN
  QSKIP("a file cannot be made unreadable for its owner on Windows");
#else
  SettingsRestorer restorer;
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral("locked"));
  {
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();
    QVERIFY(f.setPermissions(QFileDevice::Permissions()));
  }
  if (QFileInfo(file).isReadable()) {
    QFile(file).setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QSKIP("permissions are not enforced here (root or a lax filesystem)");
  }

  ConfigDialog dialog(nullptr);
  child<QLineEdit>(dialog, "sshAuthSockOverride")->setText(file);
  QString title;
  const QString text = acceptAndCollectWarning(dialog, &title);
  QFile(file).setPermissions(QFile::ReadOwner | QFile::WriteOwner);

  COMPARE_BOX_TITLE(
      title, QStringLiteral("Potentially invalid SSH_AUTH_SOCK override"));
  QVERIFY2(text.contains(QStringLiteral("not readable")), qPrintable(text));
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, file);
#endif
}

/**
 * @brief Open the dialog with no profiles, the store at @p store and the
 *        stand-in gpg as the gpg, then add one profile through the form.
 */
auto tst_configdialog::dialogWithNewProfile(const QString &store,
                                            const QString &name,
                                            const QString &path,
                                            const QString &signingKey)
    -> std::unique_ptr<ConfigDialog> {
  QtPassSettings::setProfiles(Profiles());
  {
    AppSettings s = QtPassSettings::load();
    s.passStore = store + QLatin1Char('/');
    s.usePass = false;
    s.useGit = false;
    s.gpgExecutable = m_gpg;
    s.passSigningKey.clear();
    s.activeProfile.clear();
    QtPassSettings::save(s);
  }
  auto dialog = std::make_unique<ConfigDialog>(nullptr);
  child<QLineEdit>(*dialog, "storePath")->setText(store);
  child<QLineEdit>(*dialog, "gpgPath")->setText(m_gpg);
  child<QCheckBox>(*dialog, "checkBoxUseGit")->setChecked(false);
  QMetaObject::invokeMethod(dialog.get(), "on_addButton_clicked");
  auto *nameEdit = child<QLineEdit>(*dialog, "profileName");
  nameEdit->setText(name);
  emit nameEdit->textEdited(name);
  auto *pathEdit = child<QLineEdit>(*dialog, "profilePath");
  pathEdit->setText(path);
  emit pathEdit->textEdited(path);
  auto *keyEdit = child<QLineEdit>(*dialog, "profileSigningKey");
  keyEdit->setText(signingKey);
  emit keyEdit->textEdited(signingKey);
  return dialog;
}

/**
 * @brief A new profile at a folder that does not exist asks before creating
 *        it; "No" keeps the profile but creates nothing.
 */
void tst_configdialog::newProfileDirectoryQuestionCanBeDeclined() {
  SettingsRestorer restorer;
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString path = QDir(store.path()).filePath(QStringLiteral("fresh"));
  auto dialog = dialogWithNewProfile(store.path(), QStringLiteral("fresh"),
                                     path, QString());

  QString title, text;
  ModalDriver driver([&](QWidget *modal) {
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      title = box->windowTitle();
      text = box->text();
      if (auto *no = box->button(QMessageBox::No)) {
        no->click();
        return;
      }
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  QCOMPARE(driver.seen(), 1);
  COMPARE_BOX_TITLE(title, QStringLiteral("Create profile directory?"));
  QVERIFY2(text.contains(path), qPrintable(text));
  QVERIFY2(!QDir(path).exists(), "No means no folder");
  QCOMPARE(QtPassSettings::getProfiles().value(QStringLiteral("fresh")).path,
           path);
}

/**
 * @brief "Yes" to creating the folder, but the folder cannot be made (its
 *        parent is a file): the failure is reported and nothing else runs.
 */
void tst_configdialog::newProfileDirectoryCreationFailureIsReported() {
  SettingsRestorer restorer;
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString blocker =
      QDir(store.path()).filePath(QStringLiteral("blocker"));
  {
    QFile f(blocker);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a directory");
  }
  const QString path = blocker + QStringLiteral("/sub");
  auto dialog = dialogWithNewProfile(store.path(), QStringLiteral("fresh"),
                                     path, QString());

  QStringList titles, texts;
  ModalDriver driver([&](QWidget *modal) {
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      titles << box->windowTitle();
      texts << box->text();
      if (auto *yes = box->button(QMessageBox::Yes)) {
        yes->click();
        return;
      }
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  COMPARE_BOX_TITLE(titles,
                    (QStringList{QStringLiteral("Create profile directory?"),
                                 QStringLiteral("Error")}));
  QVERIFY2(texts.last().contains(QStringLiteral("Could not create")),
           qPrintable(texts.last()));
  QVERIFY2(texts.last().contains(path), qPrintable(texts.last()));
  QVERIFY2(!QDir(path).exists(), "a failed mkpath leaves no folder behind");
}

/**
 * @brief A new profile pointing at a store that already has a .gpg-id is
 *        taken as is: no question, no recipients dialog.
 */
void tst_configdialog::newProfileWithAGpgIdNeedsNoInitialisation() {
  SettingsRestorer restorer;
  QTemporaryDir store;
  QTemporaryDir existing;
  QVERIFY(store.isValid() && existing.isValid());
  const QString gpgIdPath =
      QDir(existing.path()).filePath(QStringLiteral(".gpg-id"));
  {
    QFile gpgId(gpgIdPath);
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("0000000000000000\n");
  }
  auto dialog = dialogWithNewProfile(store.path(), QStringLiteral("ready"),
                                     existing.path(), QString());

  ModalDriver driver(
      [](QWidget *modal) { QMetaObject::invokeMethod(modal, "reject"); });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  QCOMPARE(driver.seen(), 0);
  QFile gpgId(gpgIdPath);
  QVERIFY2(gpgId.open(QIODevice::ReadOnly), ".gpg-id must still be there");
  QCOMPARE(gpgId.readAll(), QByteArrayLiteral("0000000000000000\n"));
  QCOMPARE(QtPassSettings::getProfiles().value(QStringLiteral("ready")).path,
           existing.path());
}

/**
 * @brief An existing folder without .gpg-id gets the recipients dialog,
 *        named for the profile; cancelling it leaves the folder alone.
 */
void tst_configdialog::newProfileRecipientsCancelSkipsInitialisation() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in gpg is a shell script");
#endif
  SettingsRestorer restorer;
  QTemporaryDir store;
  QTemporaryDir empty;
  QVERIFY(store.isValid() && empty.isValid());
  auto dialog = dialogWithNewProfile(store.path(), QStringLiteral("fresh"),
                                     empty.path(), QString());

  QString className, title;
  int listed = -1;
  ModalDriver driver([&](QWidget *modal) {
    className = QLatin1String(modal->metaObject()->className());
    title = modal->windowTitle();
    if (auto *list =
            modal->findChild<QListWidget *>(QStringLiteral("listWidget"))) {
      listed = list->count();
    }
    QMetaObject::invokeMethod(modal, "reject");
  });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  QCOMPARE(driver.seen(), 1);
  QCOMPARE(className, QStringLiteral("UsersDialog"));
  QCOMPARE(title, QStringLiteral("Select recipients for fresh"));
  QCOMPARE(listed, 2);
  QVERIFY2(
      !QFile::exists(QDir(empty.path()).filePath(QStringLiteral(".gpg-id"))),
      "cancel writes nothing");
}

/**
 * @brief Ticking a recipient and accepting writes that key's fingerprint to
 *        the new profile's .gpg-id; encrypted files already in the folder
 *        are left alone and the note about them is shown.
 */
void tst_configdialog::newProfileIsInitialisedWithTheChosenRecipients() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in gpg is a shell script");
#endif
  SettingsRestorer restorer;
  QTemporaryDir store;
  QTemporaryDir folder;
  QVERIFY(store.isValid() && folder.isValid());
  {
    QFile old(QDir(folder.path()).filePath(QStringLiteral("old.gpg")));
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("ciphertext");
  }
  auto dialog = dialogWithNewProfile(store.path(), QStringLiteral("fresh"),
                                     folder.path(), QString());

  QStringList classNames, texts;
  ModalDriver driver([&](QWidget *modal) {
    classNames << QLatin1String(modal->metaObject()->className());
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      texts << box->text();
      QMetaObject::invokeMethod(modal, "reject");
      return;
    }
    auto *list = modal->findChild<QListWidget *>(QStringLiteral("listWidget"));
    auto *buttons =
        modal->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    if (list == nullptr || buttons == nullptr || list->count() == 0) {
      QMetaObject::invokeMethod(modal, "reject");
      return;
    }
    list->item(0)->setCheckState(Qt::Checked);
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  QCOMPARE(classNames, (QStringList{QStringLiteral("UsersDialog"),
                                    QStringLiteral("QMessageBox")}));
  QVERIFY2(texts.first().contains(QStringLiteral("already contains")),
           qPrintable(texts.first()));
  QFile gpgId(QDir(folder.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::ReadOnly), ".gpg-id must be written");
  const QString written = QString::fromUtf8(gpgId.readAll());
  QVERIFY2(written.contains(QStringLiteral("31850CF72D9CDDE9")),
           qPrintable(written));
  QVERIFY2(!written.contains(QStringLiteral("693A0AF3FA364E76")),
           "only the ticked key is a recipient");
  QVERIFY2(
      QFile::exists(QDir(folder.path()).filePath(QStringLiteral("old.gpg"))),
      "existing files are not touched");
}

/**
 * @brief When initialising fails (here: the profile's signing key, and a gpg
 *        that writes no signature) the failure is reported under the
 *        profile's name instead of being swallowed.
 */
void tst_configdialog::newProfileInitialisationFailureIsReported() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in gpg is a shell script");
#endif
  SettingsRestorer restorer;
  QTemporaryDir store;
  QTemporaryDir folder;
  QVERIFY(store.isValid() && folder.isValid());
  auto dialog = dialogWithNewProfile(
      store.path(), QStringLiteral("signed"), folder.path(),
      QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9"));

  QStringList titles, texts;
  ModalDriver driver([&](QWidget *modal) {
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      titles << box->windowTitle();
      texts << box->text();
      QMetaObject::invokeMethod(modal, "reject");
      return;
    }
    auto *list = modal->findChild<QListWidget *>(QStringLiteral("listWidget"));
    auto *buttons =
        modal->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    if (list == nullptr || buttons == nullptr || list->count() == 0) {
      QMetaObject::invokeMethod(modal, "reject");
      return;
    }
    list->item(0)->setCheckState(Qt::Checked);
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  QVERIFY(QMetaObject::invokeMethod(dialog.get(), "on_accepted"));

  QCOMPARE(driver.seen(), 2);
  COMPARE_BOX_TITLE(titles,
                    QStringList{QStringLiteral("Could not initialise profile "
                                               "signed")});
  QVERIFY2(texts.first().contains(QStringLiteral("signature")),
           qPrintable(texts.first()));
  QVERIFY2(!QFile::exists(
               QDir(folder.path()).filePath(QStringLiteral(".gpg-id.sig"))),
           "no signature was produced");
}

QTEST_MAIN(tst_configdialog)
#include "tst_configdialog.moc"
