#ifndef PASS_H
#define PASS_H

#include <QString>
#include <QList>
#include <QProcess>
#include <QQueue>
//  TODO(bezet): extract UserInfo somewhere
#include "usersdialog.h"
#include "enums.h"


class Pass
{
public:
    Pass();
    virtual ~Pass() {}
    virtual void GitInit() = 0;
    virtual void GitPull() = 0;
    virtual void GitPush() = 0;
    virtual QProcess::ExitStatus Show(QString file, bool block=false) = 0;
    virtual void Insert(QString file, QString value, bool force) = 0;
    virtual void Remove(QString file, bool isDir) = 0;
    virtual void Init(QString path, const QList<UserInfo> &users) = 0;
    virtual QString Generate(int length, const QString& charset);

};

#endif // PASS_H
