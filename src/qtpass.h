// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include <QDialog>
#include <QObject>
#include <QPaintDevice>
#include <QProcess>
#include <QTimer>

class MainWindow;
class Pass;
class QtPass : public QObject {
  Q_OBJECT

public:
  QtPass(MainWindow *mainWindow);
  ~QtPass();

  auto init() -> bool;
  void setClippedText(const QString &, const QString &p_output = QString());
  void clearClippedText();
  void setClipboardTimer();
  auto isFreshStart() -> bool { return this->freshStart; }
  void setFreshStart(const bool &fs) { this->freshStart = fs; }

private:
  MainWindow *m_mainWindow;

  QProcess fusedav;

  QTimer clearClipboardTimer;
  QString clippedText;
  bool freshStart;

  void setMainWindow();
  void connectPassSignalHandlers(Pass *pass);
  void mountWebDav();

Q_SIGNALS:

public Q_SLOTS:
  void clearClipboard();
  void copyTextToClipboard(const QString &text);
  void showTextAsQRCode(const QString &text);

public:
  static QDialog *createQRCodePopup(const QPixmap &image);

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

  void showInTextBrowser(QString output, const QString &prefix = QString(),
                         const QString &postfix = QString());
};

#endif // SRC_QTPASS_H_
