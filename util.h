#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QProcessEnvironment>

class Util {
 public:
  static QString findBinaryInPath(QString binary);
  static QString findPasswordStore();
  static QString normalizeFolderPath(QString path);
  static bool checkConfig(QString passStore, QString passExecutable,
                          QString gpgExecutable);
  static void qSleep(int);

 private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif  // UTIL_H_
