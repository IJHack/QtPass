#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QProcessEnvironment>

class Util
{
public:
    static QString findBinaryInPath(QString);
    static QString findPasswordStore();
    static QString normalizeFolderPath(QString);

private:
    static void initialiseEnvironment();
    static QProcessEnvironment _env;
    static bool _envInitialised;
};

#endif // UTIL_H
