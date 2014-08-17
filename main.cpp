#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    SingleApplication app(argc, argv, "QtPass");
    if (app.isRunning()) {
        if (argc == 1 ) {
            app.sendMessage("show");
        } else if (argc == 2) {
            app.sendMessage(argv[1]);
        }
        return 0;
    }
   
    QCoreApplication::setOrganizationName("IJHack");
    QCoreApplication::setOrganizationDomain("ijhack.org");
    QCoreApplication::setApplicationName("QtPass");
    QCoreApplication::setApplicationVersion("0.1.0");

    //Setup and load translator for localization
    QTranslator translator;
    QString locale = QLocale::system().name();
    translator.load(QString(":localization/localization_") + locale + QString(".qm"));
    app.installTranslator(&translator);

    MainWindow w;

    app.setActiveWindow(&w);
    app.setWindowIcon(QIcon(":artwork/icon.png"));
    w.setApp(&app);
    w.checkConfig();
    w.show();

    return app.exec();
}
