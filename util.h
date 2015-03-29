#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QProcessEnvironment>

class Util
{
public:
    static QString findBinaryInPath(QString binary);
    static QString findPasswordStore();

private:
    static void initialiseEnvironment();
    static QProcessEnvironment _env;
    static bool _envInitialised;
};

#endif // UTIL_H
