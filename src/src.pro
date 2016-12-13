VERSION    = 1.2.0-pre

TEMPLATE   = app
QT        += core gui

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

macx {
    TARGET = QtPass
    QMAKE_MAC_SDK = macosx
    QT += svg
} else {
    TARGET = qtpass
}

SOURCES   += main.cpp\
             mainwindow.cpp \
             configdialog.cpp \
             storemodel.cpp \
             util.cpp \
             usersdialog.cpp \
             keygendialog.cpp \
             trayicon.cpp \
             passworddialog.cpp \
             qprogressindicator.cpp \
             qpushbuttonwithclipboard.cpp \
             qtpasssettings.cpp \
             settingsconstants.cpp \
             pass.cpp \
             realpass.cpp \
             imitatepass.cpp \
             executor.cpp

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
             qtpasssettings.h \
             enums.h \
             settingsconstants.h \
             pass.h \
             realpass.h \
             imitatepass.h \
             datahelpers.h \
             debughelper.h \
             executor.h

FORMS     += mainwindow.ui \
             configdialog.ui \
             usersdialog.ui \
             keygendialog.ui \
             passworddialog.ui

clang|gcc:QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas

nosingleapp {
    QMAKE_CXXFLAGS += -DSINGLE_APP=0
} else {
    SOURCES += singleapplication.cpp
    HEADERS += singleapplication.h
    QT      += network
    QMAKE_CXXFLAGS += -DSINGLE_APP=1
}

DEFINES += "VERSION=\"\\\"$$VERSION\\\"\""
