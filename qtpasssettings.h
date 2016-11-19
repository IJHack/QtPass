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
#include <QHash>

#include "enums.h"
#include "mainwindow.h"



class QtPassSettings : public QObject
{
    Q_OBJECT
public:
    explicit QtPassSettings(QObject *parent = 0);

    void saveAllSettings();

    QString getVersion(const QString &defaultValue = QVariant().toString());
    void setVersion(const QString &version);

    QByteArray getGeometry(const QByteArray &defaultValue = QVariant().toByteArray());
    void setGeometry(const QByteArray &geometry);

    QByteArray getSavestate(const QByteArray &defaultValue = QVariant().toByteArray());
    void setSavestate(const QByteArray &saveState);

    QPoint getPos(const QPoint &defaultValue = QVariant().toPoint());
    void setPos(const QPoint &pos);

    QSize getSize(const QSize &defaultValue = QVariant().toSize());
    void setSize(const QSize &size);

    int getSplitterLeft(const int &defaultValue = QVariant().toInt());
    void setSplitterLeft(const int &splitterLeft);

    int getSplitterRight(const int &defaultValue = QVariant().toInt());
    void setSplitterRight(const int &splitterRight);

    bool isMaximized(const bool &defaultValue = QVariant().toBool());
    void setMaximized(const bool &maximized);

    bool isUsePass(const bool &defaultValue = QVariant().toBool());
    void setUsePass(const bool &usePass);

    Enums::clipBoardType getClipBoardType(const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER);
    void setClipBoardType(const Enums::clipBoardType &clipBoardType);

    bool isUseAutoclear(const bool &defaultValue = QVariant().toBool());
    void setUseAutoclear(const bool &useAutoclear);

    int getAutoclearSeconds(const int &defaultValue = QVariant().toInt());
    void setAutoclearSeconds(const int &autoClearSeconds);

    bool isUseAutoclearPanel(const bool &defaultValue = QVariant().toBool());
    void setUseAutoclearPanel(const bool &useAutoclearPanel);

    int getAutoclearPanelSeconds(const int &defaultValue = QVariant().toInt());
    void setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

    bool isHidePassword(const bool &defaultValue = QVariant().toBool());
    void setHidePassword(const bool &hidePassword);

    bool isHideContent(const bool &defaultValue = QVariant().toBool());
    void setHideContent(const bool &hideContent);

    bool isAddGPGId(const bool &defaultValue = QVariant().toBool());
    void setAddGPGId(const bool &addGPGId);

    QString getPassStore(const QString &defaultValue = QVariant().toString());
    void setPassStore(const QString &passStore);

    QString getPassExecutable(const QString &defaultValue = QVariant().toString());
    void setPassExecutable(const QString &passExecutable);

    QString getGitExecutable(const QString &defaultValue = QVariant().toString());
    void setGitExecutable(const QString &gitExecutable);

    QString getGpgExecutable(const QString &defaultValue = QVariant().toString());
    void setGpgExecutable(const QString &gpgExecutable);

    QString getPwgenExecutable(const QString &defaultValue = QVariant().toString());
    void setPwgenExecutable(const QString &pwgenExecutable);

    QString getGpgHome(const QString &defaultValue = QVariant().toString());
    void setGpgHome(const QString &gpgHome);

    bool isUseWebDav(const bool &defaultValue = QVariant().toBool());
    void setUseWebDav(const bool &useWebDav);

    QString getWebDavUrl(const QString &defaultValue = QVariant().toString());
    void setWebDavUrl(const QString &webDavUrl);

    QString getWebDavUser(const QString &defaultValue = QVariant().toString());
    void setWebDavUser(const QString &webDavUser);

    QString getWebDavPassword(const QString &defaultValue = QVariant().toString());
    void setWebDavPassword(const QString &webDavPassword);

    QString getProfile(const QString &defaultValue = QVariant().toString());
    void setProfile(const QString &profile);

    bool isUseGit(const bool &defaultValue = QVariant().toBool());
    void setUseGit(const bool &useGit);

    bool isUsePwgen(const bool &defaultValue = QVariant().toBool());
    void setUsePwgen(const bool &usePwgen);

    bool isAvoidCapitals(const bool &defaultValue = QVariant().toBool());
    void setAvoidCapitals(const bool &avoidCapitals);

    bool isAvoidNumbers(const bool &defaultValue = QVariant().toBool());
    void setAvoidNumbers(const bool &avoidNumbers);

    bool isLessRandom(const bool &defaultValue = QVariant().toBool());
    void setLessRandom(const bool &lessRandom);

    bool isUseSymbols(const bool &defaultValue = QVariant().toBool());
    void setUseSymbols(const bool &useSymbols);

    int getPasswordCharsSelected(const int &defaultValue = QVariant().toInt());
    void setPasswordCharsSelected(const int &passwordCharsSelected);

    int getPasswordLength(const int &defaultValue = QVariant().toInt());
    void setPasswordLength(const int &passwordLength);

    int getPasswordCharsselection(const int &defaultValue = QVariant().toInt());
    void setPasswordCharsselection(const int &passwordCharsselection);

    QString getPasswordChars(const QString &defaultValue = QVariant().toString());
    void setPasswordChars(const QString &passwordChars);

    bool isUseTrayIcon(const bool &defaultValue = QVariant().toBool());
    void setUseTrayIcon(const bool &useTrayIcon);

    bool isHideOnClose(const bool &defaultValue = QVariant().toBool());
    void setHideOnClose(const bool &hideOnClose);

    bool isStartMinimized(const bool &defaultValue = QVariant().toBool());
    void setStartMinimized(const bool &startMinimized);

    bool isAlwaysOnTop(const bool &defaultValue = QVariant().toBool());
    void setAlwaysOnTop(const bool &alwaysOnTop);

    bool isAutoPull(const bool &defaultValue = QVariant().toBool());
    void setAutoPull(const bool &autoPull);

    bool isAutoPush(const bool &defaultValue = QVariant().toBool());
    void setAutoPush(const bool &autoPush);

    QString getPassTemplate(const QString &defaultValue = QVariant().toString());
    void setPassTemplate(const QString &passTemplate);

    bool isUseTemplate(const bool &defaultValue = QVariant().toBool());
    void setUseTemplate(const bool &useTemplate);

    bool isTemplateAllFields(const bool &defaultValue = QVariant().toBool());
    void setTemplateAllFields(const bool &templateAllFields);

signals:

public slots:

private:
    // member
    QScopedPointer<QSettings> settings;

    QHash<QString, QString> stringSettings;
    QHash<QString, QByteArray> byteArraySettings;
    QHash<QString, QPoint> pointSettings;
    QHash<QString, QSize> sizeSettings;
    QHash<QString, int> intSettings;
    QHash<QString, bool> boolSettings;

    // functions
    QSettings &getSettings();

    QString getStringValue(const QString &key,const QString &defaultValue);
    int getIntValue(const QString &key, const int &defaultValue);
    bool getBoolValue(const QString &key, const bool &defaultValue);
    QByteArray getByteArrayValue(const QString &key, const QByteArray &defaultValue);
    QPoint getPointValue(const QString &key, const QPoint &defaultValue);
    QSize getSizeValue(const QString &key, const QSize &defaultValue);

    void setStringValue(const QString &key,const QString &stringValue);
    void setIntValue(const QString &key, const int &intValue);
    void setBoolValue(const QString &key, const bool &boolValue);
    void setByteArrayValue(const QString &key, const QByteArray &byteArrayValue);
    void setPointValue(const QString &key, const QPoint &pointValue);
    void setSizeValue(const QString &key, const QSize &sizeValue);







};

#endif // QTPASSSETTINGS_H
