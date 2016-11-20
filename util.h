#ifndef UTIL_H_
#define UTIL_H_

#include <QProcessEnvironment>
#include <QString>

/*!
    \class Util
    \brief Some static utilities to be used elsewhere.
 */
class Util {
public:
  static QString findBinaryInPath(QString binary);
  static QString findPasswordStore();
  static QString normalizeFolderPath(QString path);
  static bool checkConfig();
  static void qSleep(int ms);

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // UTIL_H_
