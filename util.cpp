#include <QDebug>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#include "util.h"

QProcessEnvironment Util::_env;
bool Util::_envInitialised;

void Util::initialiseEnvironment()
{
    if (!_envInitialised) {
        _env = QProcessEnvironment::systemEnvironment();
        _envInitialised = true;
    }
}

QString Util::findBinaryInPath(QString binary)
{
    initialiseEnvironment();

    QString ret = "";

    binary.prepend("/");

    if (_env.contains("PATH")) {
        QString path = _env.value("PATH");

        QStringList entries = path.split(':');
        if (entries.length() < 2) {
            entries = path.split(';');
        }

        foreach(QString entry, entries) {
            QFileInfo *qfi = new QFileInfo(entry.append(binary));
            qDebug() << entry;

#ifdef WINDOWS
            if (!qfi->exists()) {
                QFileInfo qfi = new QFileInfo(entry.append(".exe"));
            }
#endif
            if (!qfi->isExecutable()) {
                continue;
            }

            ret = qfi->absoluteFilePath();
            break;
        }
    }

    return ret;
}
