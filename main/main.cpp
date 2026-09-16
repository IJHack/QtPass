// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "qtpasssettings.h"
#include "sshauthsock.h"
#if SINGLE_APP
#include "singleapplication.h"
#endif

#include <QApplication>
#include <QDir>
#include <QTranslator>
#include <QtWidgets>

/**
 * @mainpage QtPass
 *
 * @section intro_sec Introduction
 *
 * QtPass is a multi-platform GUI for pass, the standard unix password manager.
 *
 * https://qtpass.org/
 *
 * @section install_sec Installation
 *
 * @subsection dependencies Dependencies
 *
 * - QtPass requires Qt 6.2 or later.
 * - The Linguist package is required to compile the translations.
 * - For use of the fallback icons the SVG library is required.
 *
 * At runtime the only real dependency is gpg2 but to make the most of it,
 * you'll need git and pass too.
 *
 * @subsection source From source
 *
 * On most *nix systems all you need is:
 *
 * `qmake6 && make && make install`
 */

/**
 * @brief Joins all arguments from index @p start onward into a space-separated
 * string.
 * @param args Full argument list.
 * @param start Index of the first argument to include.
 * @return Space-joined string of the remaining arguments.
 */
static auto joinRemainingArgs(const QStringList &args, int start) -> QString {
  Q_ASSERT(start >= 0 && start <= args.size());
  return args.mid(start).join(" ");
}

/**
 * @brief Appends a suffix to a target string, inserting a separating space if
 * the target is not empty.
 *
 * If `suffix` is empty, `target` is left unchanged.
 *
 * @param target String to append to; modified in place.
 * @param suffix Suffix to append.
 */
static auto appendWithSpaceIfSuffixNotEmpty(QString &target,
                                            const QString &suffix) -> void {
  if (!suffix.isEmpty()) {
    if (!target.isEmpty())
      target += " ";
    target += suffix;
  }
}

/**
 * @brief Application entry point: parses arguments and launches the main
 * window.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Application exit code.
 */
auto main(int argc, char *argv[]) -> int {
  QString text;
#if SINGLE_APP
  QString name = qgetenv("USER");
  if (name.isEmpty())
    name = qgetenv("USERNAME");
  SingleApplication app(argc, argv, name + "QtPass");
#else
  QApplication app(argc, argv);
#endif

  const QStringList args = app.arguments();
  bool consumeNextArg = false;
  for (int i = 1; i < args.count(); ++i) {
    const auto &arg = args.at(i);

    if (arg == "--") {
      consumeNextArg = false;
      appendWithSpaceIfSuffixNotEmpty(text, joinRemainingArgs(args, i + 1));
      break;
    }

    if (consumeNextArg) {
      consumeNextArg = false;
      continue;
    }

    if (arg.startsWith('-')) {
      // We only collect positional arguments into `text`.
      // For options in the form `--option value`, skip the separate value
      // token. Options in the form `--option=value` are fully contained in
      // `arg`.
      const bool optionTakesSeparateValue =
          !arg.contains('=') && i + 1 < args.count() && arg.startsWith("--") &&
          !arg.startsWith("---") && !args[i + 1].startsWith('-');
      if (optionTakesSeparateValue)
        consumeNextArg = true;
      continue;
    }

    if (!text.isEmpty())
      text += " ";
    text += arg;
  }

  if ((text.indexOf("-psn_") == 0) || (text.indexOf("-session") == 0)) {
    text.clear();
  }

#if SINGLE_APP
  // Hand the arguments to the running instance. When that fails (the peer
  // vanished between the probe and now) start normally instead of exiting
  // silently, which would leave the user with a launcher click that did
  // nothing.
  if (app.isRunning() && app.sendMessage(text)) {
    return 0;
  }
#endif

  Q_INIT_RESOURCE(resources);
  Q_INIT_RESOURCE(qmake_qmake_qm_files); //  qmake names the file

  QCoreApplication::setOrganizationName("IJHack");
  QCoreApplication::setOrganizationDomain("ijhack.org");
  QCoreApplication::setApplicationName("QtPass");
  QCoreApplication::setApplicationVersion(VERSION);

  // Probe / set SSH_AUTH_SOCK before any subprocess runs (issue #543).
  // GUI launchers don't inherit shell-set env vars, so users with
  // gpg-agent's SSH support get failed git push/pull until this fires.
  SshAuthSock::initialise(QtPassSettings::load().sshAuthSockOverride);

  // Setup and load translator for localization.
  //
  // Use the QLocale-aware load() overload so Qt walks the user's preferred
  // language list (e.g. ar_MA -> ar, da_DK -> da) automatically. The previous
  // single-filename form returned false on the first miss and left the app
  // untranslated whenever the system locale carried a country suffix that
  // didn't match any .qm file verbatim.
  QTranslator translator;
  if (translator.load(QLocale::system(), QStringLiteral("localization"),
                      QStringLiteral("_"), QStringLiteral(":/localization"),
                      QStringLiteral(".qm"))) {
#if SINGLE_APP
    SingleApplication::installTranslator(&translator);
    SingleApplication::setLayoutDirection(
        QObject::tr("LTR") == "RTL" ? Qt::RightToLeft : Qt::LeftToRight);
#else
    QApplication::installTranslator(&translator);
    QApplication::setLayoutDirection(
        QObject::tr("LTR") == "RTL" ? Qt::RightToLeft : Qt::LeftToRight);
#endif
  }

  // Both must be set before the window exists: Wayland compositors read the
  // desktop file name when the surface is created and use it to find the
  // icon, and a window created before setWindowIcon() keeps the default one.
  // Inside a Flatpak the desktop file is renamed to the app id.
  QGuiApplication::setDesktopFileName(
      qEnvironmentVariable("FLATPAK_ID", QStringLiteral("qtpass")));
  QApplication::setWindowIcon(QIcon(":artwork/icon.png"));

  MainWindow w(text);

  // A cancelled first-run wizard (or otherwise unusable configuration) makes
  // QtPass::init() report failure from the MainWindow constructor. Quitting
  // there is a no-op before exec() runs, so bail here before the window is
  // ever shown.
  if (!w.initSucceeded()) {
    return 0;
  }

  w.activateWindow();

#if SINGLE_APP
  QObject::connect(&app, &SingleApplication::messageAvailable, &w,
                   &MainWindow::messageAvailable);
#endif

  // Center the MainWindow on the screen the mouse pointer is currently on
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  if (screen) {
    const QPoint cursorScreenCenter = screen->geometry().center();
    QRect windowFrameGeo = w.frameGeometry();
    windowFrameGeo.moveCenter(cursorScreenCenter);
    w.move(windowFrameGeo.topLeft());
  }

  w.show();

#if SINGLE_APP
  return SingleApplication::exec();
#else
  return QApplication::exec();
#endif
}
