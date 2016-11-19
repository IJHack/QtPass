#ifndef QTPASSSETTINGS_H
#define QTPASSSETTINGS_H

#include <QObject>
#include <QSettings>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QVariant>
#include <QByteArray>
#include <QPoint>
#include <QSize>

#include "enums.h"
#include "mainwindow.h"



class QtPassSettings : public QObject
{
    Q_OBJECT
public:
    explicit QtPassSettings(QObject *parent = 0);

    QString getVersion(const QString &defaultValue = QVariant().toString());
    QString setVersion(const QString &version);

    QByteArray getGeometry(const QByteArray &defaultValue = QVariant().toByteArray());
    QByteArray setGeometry(const QByteArray &geometry);

    QByteArray getSavestate(const QByteArray &defaultValue = QVariant().toByteArray());
    QByteArray setSavestate(const QByteArray &saveState);

    QPoint getPos(const QPoint &defaultValue = QVariant().toPoint());
    QPoint setPos(const QPoint &pos);

    QSize getSize(const QSize &defaultValue = QVariant().toSize());
    QSize setSize(const QSize &size);

    int getSplitterLeft(const int &defaultValue = QVariant().toInt());
    int setSplitterLeft(const int &splitterLeft);

    int getSplitterRight(const int &defaultValue = QVariant().toInt());
    int setSplitterRight(const int &splitterRight);

    bool isMaximized(const bool &defaultValue = QVariant().toBool());
    bool setMaximized(const bool &maximized);

    bool isUsePass(const bool &defaultValue = QVariant().toBool());
    bool setUsePass(const bool &usePass);

    Enums::clipBoardType getClipBoardType(const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER);
    Enums::clipBoardType setClipBoardType(const Enums::clipBoardType &clipBoardType);

    bool isUseAutoclear(const bool &defaultValue = QVariant().toBool());
    bool setUseAutoclear(const bool &useAutoclear);

    int getAutoclearSeconds(const int &defaultValue = QVariant().toInt());
    int setAutoclearSeconds(const int &autoClearSeconds);

    bool isUseAutoclearPanel(const bool &defaultValue = QVariant().toBool());
    bool setUseAutoclearPanel(const bool &useAutoclearPanel);

    int getAutoclearPanelSeconds(const int &defaultValue = QVariant().toInt());
    int setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

    bool isHidePassword(const bool &defaultValue = QVariant().toBool());
    bool setHidePassword(const bool &hidePassword);

    bool isHideContent(const bool &defaultValue = QVariant().toBool());
    bool setHideContent(const bool &hideContent);

    bool isAddGPGId(const bool &defaultValue = QVariant().toBool());
    bool setAddGPGId(const bool &addGPGId);

    QString getPassStore(const QString &defaultValue = QVariant().toString());
    QString setPassStore(const QString &passStore);

    QString getPassExecutable(const QString &defaultValue = QVariant().toString());
    QString setPassExecutable(const QString &passExecutable);

    QString getGitExecutable(const QString &defaultValue = QVariant().toString());
    QString setGitExecutable(const QString &gitExecutable);

    QString getGpgExecutable(const QString &defaultValue = QVariant().toString());
    QString setGpgExecutable(const QString &gpgExecutable);

    QString getPwgenExecutable(const QString &defaultValue = QVariant().toString());
    QString setPwgenExecutable(const QString &pwgenExecutable);

    QString getGpgHome(const QString &defaultValue = QVariant().toString());
    QString setGpgHome(const QString &gpgHome);

    bool isUseWebDav(const bool &defaultValue = QVariant().toBool());
    bool setUseWebDav(const bool &useWebDav);

    QString getWebDavUrl(const QString &defaultValue = QVariant().toString());
    QString setWebDavUrl(const QString &webDavUrl);

    QString getWebDavUser(const QString &defaultValue = QVariant().toString());
    QString setWebDavUser(const QString &webDavUser);

    QString getWebDavPassword(const QString &defaultValue = QVariant().toString());
    QString setWebDavPassword(const QString &webDavPassword);

    QString getProfile(const QString &defaultValue = QVariant().toString());
    QString setProfile(const QString &profile);

    bool isUseGit(const bool &defaultValue = QVariant().toBool());
    bool setUseGit(const bool &useGit);

    bool isUsePwgen(const bool &defaultValue = QVariant().toBool());
    bool setUsePwgen(const bool &usePwgen);

    bool isAvoidCapitals(const bool &defaultValue = QVariant().toBool());
    bool setAvoidCapitals(const bool &avoidCapitals);

    bool isAvoidNumbers(const bool &defaultValue = QVariant().toBool());
    bool setAvoidNumbers(const bool &AvoidNumbers);

    bool isLessRandom(const bool &defaultValue = QVariant().toBool());
    bool setLessRandom(const bool &lessRandom);

    bool isUseSymbols(const bool &defaultValue = QVariant().toBool());
    bool setUseSymbols(const bool &useSymbols);

    int getPasswordCharsSelected(const int &defaultValue = QVariant().toInt());
    int setPasswordCharsSelected(const int &passwordCharsSelected);

    int getPasswordLength(const int &defaultValue = QVariant().toInt());
    int setPasswordLength(const int &passwordLength);

    int getPasswordCharsselection(const int &defaultValue = QVariant().toInt());
    int setPasswordCharsselection(const int &passwordCharsselection);

    QString getPasswordChars(const QString &defaultValue = QVariant().toString());
    QString setPasswordChars(const QString &passwordChars);

    bool isUseTrayIcon(const bool &defaultValue = QVariant().toBool());
    bool setUseTrayIcon(const bool &useTrayIcon);

    bool isHideOnClose(const bool &defaultValue = QVariant().toBool());
    bool setHideOnClose(const bool &hideOnClose);

    bool isStartMinimized(const bool &defaultValue = QVariant().toBool());
    bool setStartMinimized(const bool &startMinimized);

    bool isAlwaysOnTop(const bool &defaultValue = QVariant().toBool());
    bool setAlwaysOnTop(const bool &alwaysOnTop);

    bool isAutoPull(const bool &defaultValue = QVariant().toBool());
    bool setAutoPull(const bool &autoPull);

    bool isAutoPush(const bool &defaultValue = QVariant().toBool());
    bool setAutoPush(const bool &autoPush);

    QString getPassTemplate(const QString &defaultValue = QVariant().toString());
    QString setPassTemplate(const QString &passTemplate);

    bool isUseTemplate(const bool &defaultValue = QVariant().toBool());
    bool setUseTemplate(const bool &useTemplate);

    bool isTemplateAllFields(const bool &defaultValue = QVariant().toBool());
    bool setTemplateAllFields(const bool &templateAllFields);

signals:

public slots:

private:
    // member
    QScopedPointer<QSettings> settings;
    // functions
    QSettings &getSettings();
    QString getStringValue(const QString &key,const QString &defaultValue);
    int getIntValue(const QString &key, const int &defaultValue);
    bool getBoolValue(const QString &key, const bool &defaultValue);
    QByteArray getByteArrayValue(const QString &key, const QByteArray &defaultValue);
    QPoint getPointValue(const QString &key, const QPoint &defaultValue);
    QSize getSizeValue(const QString &key, const QSize &defaultValue);






};

#endif // QTPASSSETTINGS_H
