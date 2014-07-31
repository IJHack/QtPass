#include "mainwindow.h"
#include <QApplication>
#include <QProcess>
//#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;

    QProcess *testProcess = new QProcess();

    testProcess->start("which pass");
    testProcess->waitForFinished();
    if (testProcess->exitCode() == 0) {
        w.setPassExecutable(testProcess->readAllStandardOutput());
    } else {
        w.setPassExecutable("");
        //QMessageBox msgBox;
        //msgBox.setText("Please install pass from http://www.passwordstore.org/");
        //msgBox.exec();
        testProcess->start("which git");
        testProcess->waitForFinished();
        if (testProcess->exitCode() == 0) {
            w.setGitExecutable(testProcess->readAllStandardOutput());
        }
        testProcess->start("which gpg2");
        testProcess->waitForFinished();
        if (testProcess->exitCode() != 0) {
            testProcess->start("which gpg");
            testProcess->waitForFinished();
        }
        if (testProcess->exitCode() == 0) {
            w.setGpgExecutable(testProcess->readAllStandardOutput());
        }
    }

    w.checkConfig();
    w.show();

    return app.exec();
}
