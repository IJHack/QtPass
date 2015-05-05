#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
#if SINGLE_APP
    SingleApplication app(argc, argv, "ijhackQtPass");
    if (app.isRunning()) {
        if (argc == 1 ) {
            app.sendMessage("show");
        } else if (argc >= 2) {
            QString text = "";
            for (int i = 1; i < argc; ++i) {
                text += argv[i];
                if (argc >= (i - 2)) {
                    text += " ";
                }
                app.sendMessage(text);
            }
        }
        return 0;
    }
#else
    QApplication app(argc, argv);
#endif
   
    QCoreApplication::setOrganizationName("IJHack");
    QCoreApplication::setOrganizationDomain("ijhack.org");
    QCoreApplication::setApplicationName("QtPass");
    QCoreApplication::setApplicationVersion("0.8.0");

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
