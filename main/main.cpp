#include "mainwindow.h"
#if SINGLE_APP
#include "singleapplication.h"
#endif

#include <QApplication>
#include <QDir>
#include <QTranslator>
#include <QtWidgets>

/*! \mainpage QtPass
 *
 * \section intro_sec Introduction
 *
 * QtPass is a multi-platform GUI for pass, the standard unix password manager.
 *
 * https://qtpass.org/
 *
 * \section install_sec Installation
 *
 * \subsection dependencies Dependencies
 *
 * - QtPass requires Qt 5.2 or later.
 * - The Linguist package is required to compile the translations.
 * - For use of the fallback icons the SVG library is required.
 *
 * At runtime the only real dependency is gpg2 but to make the most of it,
 * you'll need git and pass too.
 *
 * \subsection source From source
 *
 * On most *nix systems all you need is:
 *
 * `qmake && make && make install`
 */

/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) &&                                 \
    QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
  QString text = "";
  for (int i = 1; i < argc; ++i) {
    if (i > 1)
      text += " ";
    text += argv[i];
  }

  if ((text.indexOf("-psn_") == 0) || (text.indexOf("-session") == 0)) {
    text.clear();
  }

#if SINGLE_APP
  QString name = qgetenv("USER");
  if (name.isEmpty())
    name = qgetenv("USERNAME");
  SingleApplication app(argc, argv, name + "QtPass");
  if (app.isRunning()) {
    if (text.length() > 0)
      app.sendMessage(text);
    return 0;
  }
#else
  QApplication app(argc, argv);
#endif

  Q_INIT_RESOURCE(resources);
  Q_INIT_RESOURCE(qmake_qmake_qm_files); // qmake names the file

  QCoreApplication::setOrganizationName("IJHack");
  QCoreApplication::setOrganizationDomain("ijhack.org");
  QCoreApplication::setApplicationName("QtPass");
  QCoreApplication::setApplicationVersion(VERSION);

  // Setup and load translator for localization
  QTranslator translator;
  QString locale = QLocale::system().name();
  // locale = "nl_NL";
  // locale = "he_IL";
  // locale = "ar_MA";
  if (translator.load(
          QString(":localization/localization_%1.qm").arg(locale))) {
    SingleApplication::installTranslator(&translator);
    SingleApplication::setLayoutDirection(
        QObject::tr("LTR") == "RTL" ? Qt::RightToLeft : Qt::LeftToRight);
  }

  MainWindow w(text);

  SingleApplication::setActiveWindow(&w);
  SingleApplication::setWindowIcon(QIcon(":artwork/icon.png"));

#if SINGLE_APP
  QObject::connect(&app, &SingleApplication::messageAvailable, &w,
                   &MainWindow::messageAvailable);
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  QGuiApplication::setDesktopFileName("qtpass.desktop");
#endif

  // Center the MainWindow on the screen the mouse pointer is currently on
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  static int cursorScreen =
      app.desktop()->screenNumber(app.desktop()->cursor().pos());
  QPoint cursorScreenCenter =
      app.desktop()->screenGeometry(cursorScreen).center();
#else
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  QPoint cursorScreenCenter = screen->geometry().center();
#endif
  QRect windowFrameGeo = w.frameGeometry();
  windowFrameGeo.moveCenter(cursorScreenCenter);
  w.move(windowFrameGeo.topLeft());

  w.show();

  return SingleApplication::exec();
}
