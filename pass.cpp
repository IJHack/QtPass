#include "pass.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QTextCodec>

Pass::Pass() : wrapperRunning(false) {
  connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), this,
          SIGNAL(finished(int, QProcess::ExitStatus)));
  connect(&process, SIGNAL(error(QProcess::ProcessError)), this,
          SIGNAL(error(QProcess::ProcessError)));
  connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), this,
          SLOT(processFinished(int, QProcess::ExitStatus)));

  env = QProcess::systemEnvironment();

#ifdef __APPLE__
  // If it exists, add the gpgtools to PATH
  if (QFile("/usr/local/MacGPG2/bin").exists())
    env.replaceInStrings("PATH=", "PATH=/usr/local/MacGPG2/bin:");
  // Add missing /usr/local/bin
  if (env.filter("/usr/local/bin").isEmpty())
    env.replaceInStrings("PATH=", "PATH=/usr/local/bin:");
#endif

  if (!QtPassSettings::getGpgHome().isEmpty()) {
    QDir absHome(QtPassSettings::getGpgHome());
    absHome.makeAbsolute();
    env << "GNUPGHOME=" + absHome.path();
  }
}

QProcess::ExitStatus Pass::waitForProcess() {
  process.waitForFinished(30000);
  return process.exitStatus();
}

/**
 * @brief Pass::Generate use either pwgen or internal password
 * generator
 * @param length of the desired password
 * @param selection character set to use for generation
 * @return the password
 */
//  TODO(bezet): this should definitely throw
QString Pass::Generate(int length, const QString &charset) {
  QString passwd;
  if (QtPassSettings::isUsePwgen()) {
    waitFor(2);
    // --secure goes first as it overrides --no-* otherwise
    QString args =
        QString("-1 ") + (QtPassSettings::isLessRandom() ? "" : "--secure ") +
        (QtPassSettings::isAvoidCapitals() ? "--no-capitalize "
                                           : "--capitalize ") +
        (QtPassSettings::isAvoidNumbers() ? "--no-numerals " : "--numerals ") +
        (QtPassSettings::isUseSymbols() ? "--symbols " : "") +
        QString::number(length);
    executeWrapper(QtPassSettings::getPwgenExecutable(), args);
    process.waitForFinished(1000);
    if (process.exitStatus() == QProcess::NormalExit)
      passwd =
          QString(process.readAllStandardOutput()).remove(QRegExp("[\\n\\r]"));
    else
      qDebug() << "pwgen fail";
  } else {
    if (charset.length() > 0) {
      for (int i = 0; i < length; ++i) {
        int index = qrand() % charset.length();
        QChar nextChar = charset.at(index);
        passwd.append(nextChar);
      }
    } else {
      emit critical(
          tr("No characters chosen"),
          tr("Can't generate password, there are no characters to choose from "
             "set in the configuration!"));
    }
  }
  return passwd;
}

/**
 * @brief Pass::GenerateGPGKeys internal gpg keypair generator . .
 * @param batch
 * @param keygenWindow
 */
void Pass::GenerateGPGKeys(QString batch) {
  executeWrapper(QtPassSettings::getGpgExecutable(),
                 "--gen-key --no-tty --batch", batch);
  // TODO check status / error messages
  // https://github.com/IJHack/QtPass/issues/202#issuecomment-251081688
}

/**
 * @brief Pass::listKeys list users
 * @param keystring
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
QList<UserInfo> Pass::listKeys(QString keystring, bool secret) {
  waitFor(5);
  QList<UserInfo> users;
  QString listopt = secret ? "--list-secret-keys " : "--list-keys ";
  executeWrapper(QtPassSettings::getGpgExecutable(),
                 "--no-tty --with-colons " + listopt + keystring);
  process.waitForFinished(2000);
  if (process.exitStatus() != QProcess::NormalExit)
    return users;
  QByteArray processOutBytes = process.readAllStandardOutput();
  QTextCodec *codec = QTextCodec::codecForLocale();
  QString processOutString = codec->toUnicode(processOutBytes);
  QStringList keys = QString(processOutString)
                         .split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
  UserInfo current_user;
  foreach (QString key, keys) {
    QStringList props = key.split(':');
    if (props.size() < 10)
      continue;
    if (props[0] == (secret ? "sec" : "pub")) {
      if (!current_user.key_id.isEmpty())
        users.append(current_user);
      current_user = UserInfo();
      current_user.key_id = props[4];
      current_user.name = props[9].toUtf8();
      current_user.validity = props[1][0].toLatin1();
      current_user.created.setTime_t(props[5].toUInt());
      current_user.expiry.setTime_t(props[6].toUInt());
    } else if (current_user.name.isEmpty() && props[0] == "uid") {
      current_user.name = props[9];
    }
  }
  if (!current_user.key_id.isEmpty())
    users.append(current_user);
  return users;
}

/**
 * @brief Pass::waitFor wait until process.atEnd and execQueue.isEmpty
 * or timeout after x-seconds
 *
 * @param seconds
 */
void Pass::waitFor(uint seconds) {
  QDateTime current = QDateTime::currentDateTime();
  uint stop = current.toTime_t() + seconds;
  while (!process.atEnd() || !execQueue.isEmpty()) {
    current = QDateTime::currentDateTime();
    if (stop < current.toTime_t()) {
      emit critical(tr("Timed out"),
                    tr("Can't start process, previous one is still running!"));
    }
    Util::qSleep(100);
  }
}

/**
 * @brief Pass::processFinished process is finished, if there is another
 *        one queued up to run, start it.
 * @param exitCode
 * @param exitStatus
 */
void Pass::processFinished(int, QProcess::ExitStatus) {
  wrapperRunning = false;
  if (!execQueue.isEmpty()) {
    execQueueItem item = execQueue.dequeue();
    executeWrapper(item.app, item.args, item.input);
  }
}

/**
 * Temporary wrapper to ease refactoring, don't get used to it ;)
 */
QProcess::ProcessState Pass::state() { return process.state(); }

/**
 * @brief Pass::executeWrapper run an application, queue when needed.
 * @param app path to application to run
 * @param args required arguements
 * @param input optional input
 */
void Pass::executeWrapper(QString app, QString args, QString input) {
  // qDebug() << app + " " + args;
  // Happens a lot if e.g. git binary is not set.
  // This will result in bogus "QProcess::FailedToStart" messages,
  // also hiding legitimate errors from the gpg commands.
  if (app.isEmpty()) {
    qDebug() << "Trying to execute nothing..";
    return;
  }
  // Convert to absolute path, just in case
  app = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(app);
  if (wrapperRunning) {
    execQueueItem item;
    item.app = app;
    item.args = args;
    item.input = input;

    execQueue.enqueue(item);
    qDebug() << item.app + "," + item.args + "," + item.input;
    return;
  }
  wrapperRunning = true;
  process.setWorkingDirectory(QtPassSettings::getPassStore());
  process.setEnvironment(env);
  emit startingExecuteWrapper();
  process.start('"' + app + "\" " + args);
  if (!input.isEmpty())
    process.write(input.toUtf8());
  process.closeWriteChannel();
}

/**
 * @brief Pass::readAllStandardOutput   temporary helper
 * @return
 */
QByteArray Pass::readAllStandardOutput() {
  return process.readAllStandardOutput();
}

/**
 * @brief Pass::readAllStandardError    temporary helper
 * @return
 */
QByteArray Pass::readAllStandardError() {
  return process.readAllStandardError();
}

/**
 * @brief Pass::updateEnv update the execution environment (used when
 * switching profiles)
 */
void Pass::updateEnv() {
  QStringList store = env.filter("PASSWORD_STORE_DIR");
  // put PASSWORD_STORE_DIR in env
  if (store.isEmpty()) {
    // qDebug() << "Added PASSWORD_STORE_DIR";
    env.append("PASSWORD_STORE_DIR=" + QtPassSettings::getPassStore());
  } else {
    // qDebug() << "Update PASSWORD_STORE_DIR with " + passStore;
    env.replaceInStrings(store.first(), "PASSWORD_STORE_DIR=" +
                                            QtPassSettings::getPassStore());
  }
}

/**
 * @brief Pass::resetPasswordStoreDir   probably temporary helper
 */
void Pass::resetPasswordStoreDir() {
  // qDebug() << env;
  QStringList store = env.filter("PASSWORD_STORE_DIR");
  // put PASSWORD_STORE_DIR in env
  if (store.isEmpty()) {
    // qDebug() << "Added PASSWORD_STORE_DIR";
    env.append("PASSWORD_STORE_DIR=" + QtPassSettings::getPassStore());
  } else {
    // qDebug() << "Update PASSWORD_STORE_DIR";
    env.replaceInStrings(store.first(), "PASSWORD_STORE_DIR=" +
                                            QtPassSettings::getPassStore());
  }
}

/**
 * @brief Pass::getRecipientList return list op gpg-id's to encrypt for
 * @param for_file which file (folder) would you like recepients for
 * @return recepients gpg-id contents
 */
QStringList Pass::getRecipientList(QString for_file) {
  QDir gpgIdPath(QFileInfo(for_file.startsWith(QtPassSettings::getPassStore())
                               ? for_file
                               : QtPassSettings::getPassStore() + for_file)
                     .absoluteDir());
  bool found = false;
  while (gpgIdPath.exists() &&
         gpgIdPath.absolutePath().startsWith(QtPassSettings::getPassStore())) {
    if (QFile(gpgIdPath.absoluteFilePath(".gpg-id")).exists()) {
      found = true;
      break;
    }
    if (!gpgIdPath.cdUp())
      break;
  }
  QFile gpgId(found ? gpgIdPath.absoluteFilePath(".gpg-id")
                    : QtPassSettings::getPassStore() + ".gpg-id");
  if (!gpgId.open(QIODevice::ReadOnly | QIODevice::Text))
    return QStringList();
  QStringList recipients;
  while (!gpgId.atEnd()) {
    QString recipient(gpgId.readLine());
    recipient = recipient.trimmed();
    if (!recipient.isEmpty())
      recipients += recipient;
  }
  return recipients;
}

/**
 * @brief Pass::getRecipientString formated string for use with GPG
 * @param for_file which file (folder) would you like recepients for
 * @param separator formating separator eg: " -r "
 * @param count
 * @return recepient string
 */
QString Pass::getRecipientString(QString for_file, QString separator,
                                 int *count) {
  QString recipients_str;
  QStringList recipients_list = Pass::getRecipientList(for_file);
  if (count)
    *count = recipients_list.size();
  foreach (const QString recipient, recipients_list)
    recipients_str += separator + '"' + recipient + '"';
  return recipients_str;
}
