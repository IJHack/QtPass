#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // check for stupid apple psid or whatever flag
    QString text = "";
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            text += " ";
        }
        text += argv[i];
    }
#if SINGLE_APP
    QString name = qgetenv("USER");
    if (name.isEmpty())
        name = qgetenv("USERNAME");
    //qDebug() << name;
    SingleApplication app(argc, argv, name + "QtPass");
    if (app.isRunning()) {
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

    //Setup and load translator for localization
    QTranslator translator;
    QString locale = QLocale::system().name();
    //locale = "nl_NL";
    //locale = "he_IL";
    //locale = "ar_MA";
    translator.load(QString(":localization/localization_") + locale + QString(".qm"));
    app.installTranslator(&translator);
    app.setLayoutDirection(QObject::tr("LTR")=="RTL" ? Qt::RightToLeft : Qt::LeftToRight);

    MainWindow w;

    app.setActiveWindow(&w);
    app.setWindowIcon(QIcon(":artwork/icon.png"));
    w.setApp(&app);
    w.setText(text);
    w.show();
    return app.exec();
}
