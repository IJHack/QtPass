#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;

    w.checkConfig();
    w.show();

    return app.exec();
}
