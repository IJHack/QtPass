!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = lib
QT        += core gui
TARGET 	   = qtpass

CONFIG += c++11 staticlib
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

SOURCES   += mainwindow.cpp \
             configdialog.cpp \
             storemodel.cpp \
             util.cpp \
             usersdialog.cpp \
             keygendialog.cpp \
             trayicon.cpp \
             passworddialog.cpp \
             qprogressindicator.cpp \
             qpushbuttonwithclipboard.cpp \
             qpushbuttonasqrcode.cpp \
             qtpasssettings.cpp \
             settingsconstants.cpp \
             pass.cpp \
             realpass.cpp \
             imitatepass.cpp \
             executor.cpp \
             simpletransaction.cpp \
             filecontent.cpp \
    qtpass.cpp

HEADERS   += mainwindow.h \
             configdialog.h \
             storemodel.h \
             util.h \
             usersdialog.h \
             keygendialog.h \
             trayicon.h \
             passworddialog.h \
             qprogressindicator.h \
             deselectabletreeview.h \
             qpushbuttonwithclipboard.h \
             qpushbuttonasqrcode.h \
             qtpasssettings.h \
             enums.h \
             settingsconstants.h \
             pass.h \
             realpass.h \
             imitatepass.h \
             debughelper.h \
             executor.h \
             simpletransaction.h \
             filecontent.h \
             passwordconfiguration.h \
             userinfo.h \
    qtpass.h

FORMS     += mainwindow.ui \
             configdialog.ui \
             usersdialog.ui \
             keygendialog.ui \
             passworddialog.ui

updateqm.input = TRANSLATIONS
updateqm.output = ../localization/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ../localization/${QMAKE_FILE_BASE}.qm
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm
PRE_TARGETDEPS += compiler_updateqm_make_all

!nosingleapp {
    SOURCES += singleapplication.cpp
    HEADERS += singleapplication.h
}

RESOURCES   += ../resources.qrc
