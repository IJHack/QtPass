#ifndef UTIL_H_
#define UTIL_H_

#include "storemodel.h"
#include <QFileSystemModel>
#include <QProcessEnvironment>
#include <QString>

class StoreModel;
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
  static QString getDir(const QModelIndex &index, bool forPass,
                        const QFileSystemModel &model,
                        const StoreModel &storeModel);
  static void copyDir(const QString src, const QString dest);\
  static int rand();

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // UTIL_H_
