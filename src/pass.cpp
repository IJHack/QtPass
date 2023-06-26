#include "pass.h"
#include "qtpasssettings.h"
#include "util.h"

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using namespace std;
using namespace Enums;

/**
 * @brief Pass::Pass wrapper for using either pass or the pass imitation
 */
Pass::Pass() : wrapperRunning(false), env(QProcess::systemEnvironment()) {
  connect(&exec,
          static_cast<void (Executor::*)(int, int, const QString &,
                                         const QString &)>(&Executor::finished),
          this, &Pass::finished);

  // TODO(bezet): stop using process
  // connect(&process, SIGNAL(error(QProcess::ProcessError)), this,
  //        SIGNAL(error(QProcess::ProcessError)));

  connect(&exec, &Executor::starting, this, &Pass::startingExecuteWrapper);
  env.append("WSLENV=PASSWORD_STORE_DIR/p");
}

void Pass::executeWrapper(PROCESS id, const QString &app,
                          const QStringList &args, bool readStdout,
                          bool readStderr) {
  executeWrapper(id, app, args, QString(), readStdout, readStderr);
}

void Pass::executeWrapper(PROCESS id, const QString &app,
                          const QStringList &args, QString input,
                          bool readStdout, bool readStderr) {
#ifdef QT_DEBUG
  dbg() << app << args;
#endif
  exec.execute(id, QtPassSettings::getPassStore(), app, args, input, readStdout,
               readStderr);
}

void Pass::init() {
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
 * @param charset to use for generation
 * @return the password
 */
QString Pass::Generate_b(unsigned int length, const QString &charset) {
  QString passwd;
  if (QtPassSettings::isUsePwgen()) {
    // --secure goes first as it overrides --no-* otherwise
    QStringList args;
    args.append("-1");
    if (!QtPassSettings::isLessRandom())
      args.append("--secure");
    args.append(QtPassSettings::isAvoidCapitals() ? "--no-capitalize"
                                                  : "--capitalize");
    args.append(QtPassSettings::isAvoidNumbers() ? "--no-numerals"
                                                 : "--numerals");
    if (QtPassSettings::isUseSymbols())
      args.append("--symbols");
    args.append(QString::number(length));
    QString p_out;
    //  TODO(bezet): try-catch here(2 statuses to merge o_O)
    if (exec.executeBlocking(QtPassSettings::getPwgenExecutable(), args,
                             &passwd) == 0)
      passwd.remove(QRegularExpression("[\\n\\r]"));
    else {
      passwd.clear();
#ifdef QT_DEBUG
      qDebug() << __FILE__ << ":" << __LINE__ << "\t"
               << "pwgen fail";
#endif
      //    TODO(bezet): emit critical ?
    }
  } else {
    if (charset.length() > 0) {
      passwd = generateRandomPassword(charset, length);
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
 * @param batch GnuPG style configuration string
 */
void Pass::GenerateGPGKeys(QString batch) {
  executeWrapper(GPG_GENKEYS, QtPassSettings::getGpgExecutable(),
                 {"--gen-key", "--no-tty", "--batch"}, batch);
  // TODO check status / error messages - probably not here, it's just started
  // here, see finished for details
  // https://github.com/IJHack/QtPass/issues/202#issuecomment-251081688
}

/**
 * @brief Pass::listKeys list users
 * @param keystrings
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
QList<UserInfo> Pass::listKeys(QStringList keystrings, bool secret) {
  QList<UserInfo> users;
  QStringList args = {"--no-tty", "--with-colons", "--with-fingerprint"};
  args.append(secret ? "--list-secret-keys" : "--list-keys");

  foreach (QString keystring, keystrings) {
    if (!keystring.isEmpty()) {
      args.append(keystring);
    }
  }
  QString p_out;
  if (exec.executeBlocking(QtPassSettings::getGpgExecutable(), args, &p_out) !=
      0)
    return users;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList keys =
      p_out.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
#else
  QStringList keys =
      p_out.split(QRegularExpression("[\r\n]"), QString::SkipEmptyParts);
#endif
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
      current_user.created.setSecsSinceEpoch(props[5].toUInt());
      current_user.expiry.setSecsSinceEpoch(props[6].toUInt());
    } else if (current_user.name.isEmpty() && props[0] == "uid") {
      current_user.name = props[9];
    } else if ((props[0] == "fpr") && props[9].endsWith(current_user.key_id)) {
      current_user.key_id = props[9];
    }
  }
  if (!current_user.key_id.isEmpty())
    users.append(current_user);
  return users;
}

/**
 * @brief Pass::listKeys list users
 * @param keystring
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
QList<UserInfo> Pass::listKeys(QString keystring, bool secret) {
  return listKeys(QStringList(keystring), secret);
}

/**
 * @brief Pass::processFinished reemits specific signal based on what process
 * has finished
 * @param id    id of Pass process that was scheduled and finished
 * @param exitCode  return code of a process
 * @param out   output generated by process(if capturing was requested, empty
 *              otherwise)
 * @param err   error output generated by process(if capturing was requested,
 *              or error occurred)
 */
void Pass::finished(int id, int exitCode, const QString &out,
                    const QString &err) {
  auto pid = static_cast<PROCESS>(id);
  if (exitCode != 0) {
    emit processErrorExit(exitCode, err);
    return;
  }
  switch (pid) {
  case GIT_INIT:
    emit finishedGitInit(out, err);
    break;
  case GIT_PULL:
    emit finishedGitPull(out, err);
    break;
  case GIT_PUSH:
    emit finishedGitPush(out, err);
    break;
  case PASS_SHOW:
    emit finishedShow(out);
    break;
  case PASS_OTP_GENERATE:
    emit finishedOtpGenerate(out);
    break;
  case PASS_INSERT:
    emit finishedInsert(out, err);
    break;
  case PASS_REMOVE:
    emit finishedRemove(out, err);
    break;
  case PASS_INIT:
    emit finishedInit(out, err);
    break;
  case PASS_MOVE:
    emit finishedMove(out, err);
    break;
  case PASS_COPY:
    emit finishedCopy(out, err);
    break;
  default:
#ifdef QT_DEBUG
    dbg() << "Unhandled process type" << pid;
#endif
    break;
  }
}

/**
 * @brief Pass::updateEnv update the execution environment (used when
 * switching profiles)
 */
void Pass::updateEnv() {
  // put PASSWORD_STORE_SIGNING_KEY in env
  QStringList envSigningKey = env.filter("PASSWORD_STORE_SIGNING_KEY=");
  QString currentSigningKey = QtPassSettings::getPassSigningKey();
  if (envSigningKey.isEmpty()) {
    if (!currentSigningKey.isEmpty()) {
      // dbg()<< "Added
      // PASSWORD_STORE_SIGNING_KEY with" + currentSigningKey;
      env.append("PASSWORD_STORE_SIGNING_KEY=" + currentSigningKey);
    }
  } else {
    if (currentSigningKey.isEmpty()) {
      // dbg() << "Removed
      // PASSWORD_STORE_SIGNING_KEY";
      env.removeAll(envSigningKey.first());
    } else {
      // dbg()<< "Update
      // PASSWORD_STORE_SIGNING_KEY with " + currentSigningKey;
      env.replaceInStrings(envSigningKey.first(),
                           "PASSWORD_STORE_SIGNING_KEY=" + currentSigningKey);
    }
  }
  // put PASSWORD_STORE_DIR in env
  QStringList store = env.filter("PASSWORD_STORE_DIR=");
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
 * @brief Pass::getGpgIdPath return gpgid file path for some file (folder).
 * @param for_file which file (folder) would you like the gpgid file path for.
 * @return path to the gpgid file.
 */
QString Pass::getGpgIdPath(QString for_file) {
  QDir gpgIdDir(QFileInfo(for_file.startsWith(QtPassSettings::getPassStore())
                              ? for_file
                              : QtPassSettings::getPassStore() + for_file)
                    .absoluteDir());
  bool found = false;
  while (gpgIdDir.exists() &&
         gpgIdDir.absolutePath().startsWith(QtPassSettings::getPassStore())) {
    if (QFile(gpgIdDir.absoluteFilePath(".gpg-id")).exists()) {
      found = true;
      break;
    }
    if (!gpgIdDir.cdUp())
      break;
  }
  QString gpgIdPath(found ? gpgIdDir.absoluteFilePath(".gpg-id")
                          : QtPassSettings::getPassStore() + ".gpg-id");

  return gpgIdPath;
}

/**
 * @brief Pass::getRecipientList return list of gpg-id's to encrypt for
 * @param for_file which file (folder) would you like recepients for
 * @return recepients gpg-id contents
 */
QStringList Pass::getRecipientList(QString for_file) {
  QFile gpgId(getGpgIdPath(for_file));
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
QStringList Pass::getRecipientString(QString for_file, QString separator,
                                     int *count) {
  Q_UNUSED(separator)
  Q_UNUSED(count)
  return Pass::getRecipientList(for_file);
}

/* Copyright (C) 2017 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

quint32 Pass::boundedRandom(quint32 bound) {
  if (bound < 2) {
    return 0;
  }

  quint32 randval;
  const quint32 max_mod_bound = (1 + ~bound) % bound;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  static int fd = -1;
  if (fd == -1) {
    assert((fd = open("/dev/urandom", O_RDONLY)) >= 0);
  }
#endif

  do {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    assert(read(fd, &randval, sizeof(randval)) == sizeof(randval));
#else
    randval = QRandomGenerator::system()->generate();
#endif
  } while (randval < max_mod_bound);

  return randval % bound;
}

QString Pass::generateRandomPassword(const QString &charset,
                                     unsigned int length) {
  QString out;
  for (unsigned int i = 0; i < length; ++i) {
    out.append(charset.at(static_cast<int>(
        boundedRandom(static_cast<quint32>(charset.length())))));
  }
  return out;
}
