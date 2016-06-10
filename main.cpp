#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

/*! \mainpage QtPass
 *
 * \section intro_sec Introduction
 *
 * QtPass is a multi-platform GUI for pass, the standard unix password manager.
 *
 * \section install_sec Installation
 *
 * \subsection dependencies Dependencies
 *
 * - QtPass requires Qt 4.8 or later, preferably Qt5.5 or later.
 * - The Linguist package is required to compile the translations.
 * - For use of the fallback icons the SVG library is required.
 *
 * At runtime the only real dependency is gpg2 but to make the most of it, you'll need git and pass too.
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
  // check for stupid apple psid or whatever flag
  QString text = "";
  for (int i = 1; i < argc; ++i) {
    if (i > 1)
      text += " ";
    text += argv[i];
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
  translator.load(QString(":localization/localization_") + locale +
                  QString(".qm"));
  app.installTranslator(&translator);
  app.setLayoutDirection(QObject::tr("LTR") == "RTL" ? Qt::RightToLeft
                                                     : Qt::LeftToRight);
  MainWindow w;

  QObject::connect(&app, SIGNAL(aboutToQuit()), &w, SLOT(clearClipboard()));

  app.setActiveWindow(&w);
  app.setWindowIcon(QIcon(":artwork/icon.png"));
  w.setApp(&app);
  w.setText(text);
  w.show();

  return app.exec();
}
