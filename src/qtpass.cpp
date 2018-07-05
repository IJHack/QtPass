#include "qtpass.h"
#include "mainwindow.h"
#include "qtpasssettings.h"

#include <QDebug>
#include <QProcess>

QtPass::QtPass() {
  // This should connect Pass to MainWindow

  connectPassSignalHandlers(QtPassSettings::getRealPass());
  connectPassSignalHandlers(QtPassSettings::getImitatePass());
}

void QtPass::connectPassSignalHandlers(Pass *pass) {
  connect(pass, &Pass::error, this, &QtPass::processError);
  connect(pass, &Pass::processErrorExit, this, &QtPass::processErrorExit);
  connect(pass, &Pass::startingExecuteWrapper, m_mainWindow,
          &MainWindow::executeWrapperStarted);
  connect(pass, &Pass::critical, m_mainWindow, &MainWindow::critical);
  connect(pass, &Pass::statusMsg, m_mainWindow, &MainWindow::showStatusMessage);
  connect(pass, &Pass::finishedShow, m_mainWindow,
          &MainWindow::passShowHandler);
  connect(pass, &Pass::finishedOtpGenerate, m_mainWindow,
          &MainWindow::passOtpHandler);
  connect(pass, &Pass::finishedGitInit, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedGitPull, this, &QtPass::processFinished);
  connect(pass, &Pass::finishedGitPush, this, &QtPass::processFinished);
  connect(pass, &Pass::finishedInsert, this, &QtPass::finishedInsert);
  connect(pass, &Pass::finishedRemove, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedInit, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedMove, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedCopy, this, &QtPass::passStoreChanged);

  connect(pass, &Pass::finishedGenerateGPGKeys, this,
          &QtPass::onKeyGenerationComplete);

  connect(m_mainWindow, &MainWindow::passShowHandlerFinished, this,
          &QtPass::passShowHandlerFinished);
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

void QtPass::processErrorExit(int exitCode, const QString &p_error) {
  if (!p_error.isEmpty()) {
    QString output;
    QString error = p_error;
    error.replace(QRegExp("<"), "&lt;");
    error.replace(QRegExp(">"), "&gt;");
    error.replace(QRegExp(" "), "&nbsp;");
    if (exitCode == 0) {
      //  https://github.com/IJHack/qtpass/issues/111
      output = "<span style=\"color: darkgray;\">" + error + "</span><br />";
    } else {
      output = "<span style=\"color: red;\">" + error + "</span><br />";
    }

    output.replace(
        QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
        "<a href=\"\\1\">\\1</a>");
    output.replace(QRegExp("\n"), "<br />");

    m_mainWindow->flashText(output, false, true);
  }

  m_mainWindow->setUiElementsEnabled(true);
}

/**
 * @brief QtPass::processFinished background process has finished
 * @param exitCode
 * @param exitStatus
 * @param output    stdout from a process
 * @param errout    stderr from a process
 */
void QtPass::processFinished(const QString &p_output, const QString &p_errout) {
  showInTextBrowser(p_output);
  //    Sometimes there is error output even with 0 exit code, which is
  //    assumed in this function
  processErrorExit(0, p_errout);

  m_mainWindow->setUiElementsEnabled(true);
}

void QtPass::passStoreChanged(const QString &p_out, const QString &p_err) {
  processFinished(p_out, p_err);
  doGitPush();
}

void QtPass::finishedInsert(const QString &p_output, const QString &p_errout) {
  processFinished(p_output, p_errout);
  doGitPush();
  m_mainWindow->on_treeView_clicked(m_mainWindow->getCurrentTreeViewIndex());
}

void QtPass::doGitPush() {
  if (QtPassSettings::isAutoPush())
    m_mainWindow->onPush();
}

void QtPass::onKeyGenerationComplete(const QString &p_output,
                                     const QString &p_errout) {
  // qDebug() << p_output;
  // qDebug() << p_errout;
  if (0 != m_mainWindow->getKeygenDialog()) {
    qDebug() << "Keygen Done";
    m_mainWindow->cleanKeygenDialog();
    // TODO(annejan) some sanity checking ?
  }

  this->processFinished(p_output, p_errout);
}

void QtPass::passShowHandlerFinished(QString output) {
  showInTextBrowser(output);
}

void QtPass::showInTextBrowser(QString output, QString prefix,
                               QString postfix) {
  output.replace(QRegExp("<"), "&lt;");
  output.replace(QRegExp(">"), "&gt;");
  output.replace(QRegExp(" "), "&nbsp;");

  output.replace(
      QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
      "<a href=\"\\1\">\\1</a>");
  output.replace(QRegExp("\n"), "<br />");
  output = prefix + output + postfix;

  m_mainWindow->flashText(output, false, true);
}
