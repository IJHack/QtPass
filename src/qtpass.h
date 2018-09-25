#ifndef QTPASS_H
#define QTPASS_H

#include <QObject>
#include <QProcess>
#include <QTimer>

class MainWindow;
class Pass;
class QtPass : public QObject {
  Q_OBJECT

public:
  QtPass();
  ~QtPass();

  void setMainWindow(MainWindow *mW);
  void setClippedText(const QString &, const QString &p_output = QString());
  void clearClippedText();
  void setClipboardTimer();
  bool isFreshStart() { return this->freshStart; }
  void setFreshStart(const bool &fs) { this->freshStart = fs; }

private:
  MainWindow *m_mainWindow;

  QProcess fusedav;

  QTimer clearClipboardTimer;
  QString clippedText;
  bool freshStart;

  bool setup();
  void connectPassSignalHandlers(Pass *pass);
  void mountWebDav();

signals:

public slots:
  void clearClipboard();
  void copyTextToClipboard(const QString &text);

private slots:
  void processError(QProcess::ProcessError);
  void processErrorExit(int exitCode, const QString &);
  void processFinished(const QString &, const QString &);

  void passStoreChanged(const QString &, const QString &);
  void passShowHandlerFinished(QString output);

  void doGitPush();
  void finishedInsert(const QString &, const QString &);
  void onKeyGenerationComplete(const QString &p_output,
                               const QString &p_errout);

  void showInTextBrowser(QString toShow, QString prefix = QString(),
                         QString postfix = QString());
};

#endif // QTPASS_H
