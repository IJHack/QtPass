#include "pass.h"
#include "debughelper.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QTextCodec>

Pass::Pass() : wrapperRunning(false), env(QProcess::systemEnvironment()) {
  connect(&exec, SIGNAL(finished(int, const QString &, const QString &)), this,
          SIGNAL(finished(int, const QString &, const QString &)));
  // TODO(bezet): stop using process
  // connect(&process, SIGNAL(error(QProcess::ProcessError)), this,
  //        SIGNAL(error(QProcess::ProcessError)));

  connect(&exec, &Executor::starting, this, &Pass::startingExecuteWrapper);

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
    // --secure goes first as it overrides --no-* otherwise
    QStringList args;
    args.append("-1");
    if (QtPassSettings::isLessRandom())
      args.append("--secure ");
    args.append(QtPassSettings::isAvoidCapitals() ? "--no-capitalize "
                                                  : "--capitalize ");
    args.append(QtPassSettings::isAvoidNumbers() ? "--no-numerals "
                                                 : "--numerals ");
    if (QtPassSettings::isUseSymbols())
      args.append("--symbols ");
    args.append(QString::number(length));
    QString p_out;
    //  TODO(bezet): try-catch here(2 statuses to merge o_O)
    if (exec.executeBlocking(QtPassSettings::getPwgenExecutable(), args,
                             &passwd) == 0)
      passwd.remove(QRegExp("[\\n\\r]"));
    else {
      passwd.clear();
      qDebug() << __FILE__ << ":" << __LINE__ << "\t"
               << "pwgen fail";
      //    TODO(bezet): emit critical ?
    }
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
 * @param batch     GnuPG style configuration string
 * @param keygenWindow
 */
void Pass::GenerateGPGKeys(QString batch) {
  exec.execute(PASSWD_GENERATE, QtPassSettings::getGpgExecutable(),
               {"--gen-key", "--no-tty", "--batch"}, batch);
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
  QList<UserInfo> users;
  QStringList args = {"--no-tty", "--with-colons"};
  args.append(secret ? "--list-secret-keys" : "--list-keys");
  if (!keystring.isEmpty())
    args.append(keystring);
  QString p_out;
  if (exec.executeBlocking(QtPassSettings::getGpgExecutable(), args, &p_out) !=
      0)
    return users;
  QStringList keys = p_out.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
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
 * @brief Pass::updateEnv update the execution environment (used when
 * switching profiles)
 */
void Pass::updateEnv() {
  QStringList store = env.filter("PASSWORD_STORE_DIR");
  // put PASSWORD_STORE_DIR in env
  if (store.isEmpty()) {
    // dbg()<< "Added
    // PASSWORD_STORE_DIR";
    env.append("PASSWORD_STORE_DIR=" + QtPassSettings::getPassStore());
  } else {
    // dbg()<< "Update
    // PASSWORD_STORE_DIR with " + passStore;
    env.replaceInStrings(store.first(), "PASSWORD_STORE_DIR=" +
                                            QtPassSettings::getPassStore());
  }
  exec.setEnvironment(env);
}

/**
 * @brief Pass::getRecipientList return list of gpg-id's to encrypt for
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
