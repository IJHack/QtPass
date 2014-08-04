#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
   
    //Setup and load translator for localization
    QTranslator translator;
    QString locale = QLocale::system().name();
    translator.load(QString(":localization/localization_") + locale + QString(".qm"));
    app.installTranslator(&translator);
    
    MainWindow w;

    app.setWindowIcon(QIcon(":artwork/icon.png"));

    w.checkConfig();
    w.show();

    return app.exec();
}
