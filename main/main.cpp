// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
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
 * - QtPass requires Qt 6.x or later (Qt 5.15 retained for legacy support).
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
 * `qmake6 && make && make install` (use `qmake` for Qt5 legacy builds)
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

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
  if (app.isRunning()) {
    app.sendMessage(text);
    return 0;
  }
#endif

  Q_INIT_RESOURCE(resources);
  Q_INIT_RESOURCE(qmake_qmake_qm_files); //  qmake names the file

  QCoreApplication::setOrganizationName("IJHack");
  QCoreApplication::setOrganizationDomain("ijhack.org");
  QCoreApplication::setApplicationName("QtPass");
  QCoreApplication::setApplicationVersion(VERSION);

  // Setup and load translator for localization
  QTranslator translator;
  QString locale = QLocale::system().name();
  if (translator.load(
          QString(":localization/localization_%1.qm").arg(locale))) {
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

  MainWindow w(text);

  w.activateWindow();

  QApplication::setWindowIcon(QIcon(":artwork/icon.png"));

#if SINGLE_APP
  QObject::connect(&app, &SingleApplication::messageAvailable, &w,
                   &MainWindow::messageAvailable);
#endif

  QGuiApplication::setDesktopFileName("qtpass");

  // Center the MainWindow on the screen the mouse pointer is currently on
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
  static int cursorScreen =
      app.desktop()->screenNumber(app.desktop()->cursor().pos());
  QPoint cursorScreenCenter =
      app.desktop()->screenGeometry(cursorScreen).center();
  QRect windowFrameGeo = w.frameGeometry();
  windowFrameGeo.moveCenter(cursorScreenCenter);
  w.move(windowFrameGeo.topLeft());
#else
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  if (screen) {
    const QPoint cursorScreenCenter = screen->geometry().center();
    QRect windowFrameGeo = w.frameGeometry();
    windowFrameGeo.moveCenter(cursorScreenCenter);
    w.move(windowFrameGeo.topLeft());
  }
#endif

  w.show();

#if SINGLE_APP
  return SingleApplication::exec();
#else
  return QApplication::exec();
#endif
}
