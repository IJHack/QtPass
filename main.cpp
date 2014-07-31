#include "mainwindow.h"
#include <QApplication>
#include <QProcess>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;

    QProcess *testProcess = new QProcess();
    if (testProcess->execute("pass") == 0) {
        w.show();
        return app.exec();
    } else {
        QMessageBox msgBox;
        msgBox.setText("Please install pass from http://www.passwordstore.org/");
        msgBox.exec();
    }
}
