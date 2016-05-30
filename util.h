#ifndef UTIL_H_
#define UTIL_H_

#include <QProcessEnvironment>
#include <QString>

class Util {
public:
  static QString findBinaryInPath(QString binary);
  static QString findPasswordStore();
  static QString normalizeFolderPath(QString path);
  static bool checkConfig(QString passStore, QString passExecutable,
                          QString gpgExecutable);
  static void qSleep(int ms);

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // UTIL_H_
