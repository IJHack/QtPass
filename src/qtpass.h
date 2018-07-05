#ifndef QTPASS_H
#define QTPASS_H

#include <QObject>
#include <QProcess>

class MainWindow;
class Pass;
class QtPass : public QObject {
  Q_OBJECT

public:
  QtPass();

  void setMainWindow(MainWindow *mW) { m_mainWindow = mW; }

private:
  MainWindow *m_mainWindow;

  void connectPassSignalHandlers(Pass *pass);

signals:

public slots:

private slots:
  void processError(QProcess::ProcessError);
  void processErrorExit(int exitCode, const QString &);
  void processFinished(const QString &, const QString &);

  void passStoreChanged(const QString &, const QString &);
  void passShowHandlerFinished(QString output);

  void finishedInsert(const QString &, const QString &);
  void onKeyGenerationComplete(const QString &p_output,
                               const QString &p_errout);

  void doGitPush();
  void showInTextBrowser(QString toShow, QString prefix = QString(),
                         QString postfix = QString());
};

#endif // QTPASS_H
