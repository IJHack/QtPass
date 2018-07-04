#include "mainwindow.h"
#include "qtpass.h"
#include "qtpasssettings.h"

#include <QProcess>

QtPass::QtPass() {
    // This should connect Pass to MainWindow

    connectPassSignalHandlers(QtPassSettings::getRealPass());
    connectPassSignalHandlers(QtPassSettings::getImitatePass());
}

void QtPass::connectPassSignalHandlers(Pass *pass) {
    connect(pass, &Pass::error, this, &QtPass::processError);
    connect(pass, &Pass::startingExecuteWrapper, this,
            &MainWindow::executeWrapperStarted);
    connect(pass, &Pass::critical, this, &MainWindow::critical);
    connect(pass, &Pass::statusMsg, this, &MainWindow::showStatusMessage);
    connect(pass, &Pass::processErrorExit, this, &MainWindow::processErrorExit);

//    connect(pass, &Pass::finishedGitInit, this, &MainWindow::passStoreChanged);
//    connect(pass, &Pass::finishedGitPull, this, &MainWindow::processFinished);
//    connect(pass, &Pass::finishedGitPush, this, &MainWindow::processFinished);
//    connect(pass, &Pass::finishedShow, this, &MainWindow::passShowHandler);
//    connect(pass, &Pass::finishedInsert, this, &MainWindow::finishedInsert);
//    connect(pass, &Pass::finishedRemove, this, &MainWindow::passStoreChanged);
//    connect(pass, &Pass::finishedInit, this, &MainWindow::passStoreChanged);
//    connect(pass, &Pass::finishedMove, this, &MainWindow::passStoreChanged);
//    connect(pass, &Pass::finishedCopy, this, &MainWindow::passStoreChanged);

//    connect(pass, &Pass::finishedGenerateGPGKeys, this,
//            &MainWindow::keyGenerationComplete);
}

/**
 * @brief QtPass::processError something went wrong
 * @param error
 */
void QtPass::processError(QProcess::ProcessError error) {
  QString errorString;
  switch (error) {
      case QProcess::FailedToStart:
        errorString = tr("QProcess::FailedToStart");
        break;
      case QProcess::Crashed:
        errorString = tr("QProcess::Crashed");
        break;
      case QProcess::Timedout:
        errorString = tr("QProcess::Timedout");
        break;
      case QProcess::ReadError:
        errorString = tr("QProcess::ReadError");
        break;
      case QProcess::WriteError:
        errorString = tr("QProcess::WriteError");
        break;
      case QProcess::UnknownError:
        errorString = tr("QProcess::UnknownError");
        break;
  }

  m_mainWindow->flashText(errorString, true);
  m_mainWindow->setUiElementsEnabled(true);
}
