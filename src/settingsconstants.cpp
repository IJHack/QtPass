#include "settingsconstants.h"

SettingsConstants::SettingsConstants() {}

const QString SettingsConstants::version = "version";

const QString SettingsConstants::groupMainwindow = "mainwindow";
const QString SettingsConstants::geometry = SettingsConstants::groupMainwindow + "/geometry";
const QString SettingsConstants::savestate = SettingsConstants::groupMainwindow + "/savestate";
const QString SettingsConstants::pos = SettingsConstants::groupMainwindow + "/pos";
const QString SettingsConstants::size = SettingsConstants::groupMainwindow + "/size";
const QString SettingsConstants::splitterLeft = SettingsConstants::groupMainwindow + "/splitterLeft";
const QString SettingsConstants::splitterRight = SettingsConstants::groupMainwindow + "/splitterRight";
const QString SettingsConstants::maximized = SettingsConstants::groupMainwindow + "/maximized";

const QString SettingsConstants::usePass = "usePass";
const QString SettingsConstants::useSelection = "useSelection";
const QString SettingsConstants::useAutoclear = "useAutoclear";
const QString SettingsConstants::autoclearSeconds = "autoclearSeconds";
const QString SettingsConstants::useAutoclearPanel = "useAutoclearPanel";
const QString SettingsConstants::autoclearPanelSeconds =
    "autoclearPanelSeconds";
const QString SettingsConstants::hidePassword = "hidePassword";
const QString SettingsConstants::hideContent = "hideContent";
const QString SettingsConstants::addGPGId = "addGPGId";
const QString SettingsConstants::passStore = "passStore";
const QString SettingsConstants::passExecutable = "passExecutable";
const QString SettingsConstants::gitExecutable = "gitExecutable";
const QString SettingsConstants::gpgExecutable = "gpgExecutable";
const QString SettingsConstants::pwgenExecutable = "pwgenExecutable";
const QString SettingsConstants::gpgHome = "gpgHome";
const QString SettingsConstants::useWebDav = "useWebDav";
const QString SettingsConstants::webDavUrl = "webDavUrl";
const QString SettingsConstants::webDavUser = "webDavUser";
const QString SettingsConstants::webDavPassword = "webDavPassword";
const QString SettingsConstants::profile = "profile";
const QString SettingsConstants::groupProfiles = "profiles";
const QString SettingsConstants::useGit = "useGit";
const QString SettingsConstants::useClipboard = "useClipboard";
const QString SettingsConstants::usePwgen = "usePwgen";
const QString SettingsConstants::avoidCapitals = "avoidCapitals";
const QString SettingsConstants::avoidNumbers = "avoidNumbers";
const QString SettingsConstants::lessRandom = "lessRandom";
const QString SettingsConstants::useSymbols = "useSymbols";
const QString SettingsConstants::passwordLength = "passwordLength";
const QString SettingsConstants::passwordCharsselection =
    "passwordCharsselection";
const QString SettingsConstants::passwordChars = "passwordChars";
const QString SettingsConstants::useTrayIcon = "useTrayIcon";
const QString SettingsConstants::hideOnClose = "hideOnClose";
const QString SettingsConstants::startMinimized = "startMinimized";
const QString SettingsConstants::alwaysOnTop = "alwaysOnTop";
const QString SettingsConstants::autoPull = "autoPull";
const QString SettingsConstants::autoPush = "autoPush";
const QString SettingsConstants::passTemplate = "passTemplate";
const QString SettingsConstants::useTemplate = "useTemplate";
const QString SettingsConstants::templateAllFields = "templateAllFields";
const QString SettingsConstants::clipBoardType = "clipBoardType";
