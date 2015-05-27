#-------------------------------------------------
#
#   QtPass is a GUI for pass,
#           the standard unix password manager.
#
# Project created by QtCreator 2014-07-30T21:56:15
#
#-------------------------------------------------

QT        += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += c++11

macx {
    TARGET = QtPass
} else {
    TARGET = qtpass
}

TEMPLATE   = app
VERSION    = 0.8.3

SOURCES   += main.cpp\
             mainwindow.cpp \
             dialog.cpp \
             storemodel.cpp \
             util.cpp \
             usersdialog.cpp \ 
             keygendialog.cpp \
             progressindicator.cpp

HEADERS   += mainwindow.h \
             dialog.h \
             storemodel.h \
             util.h \
             usersdialog.h \
             keygendialog.h \
             progressindicator.h

FORMS     += mainwindow.ui \
             dialog.ui \
             usersdialog.ui \ 
             keygendialog.ui

*-g++* {
    QMAKE_CXXFLAGS += -std=c++11 
}

nosingleapp {
    QMAKE_CXXFLAGS += -DSINGLE_APP=0
} else {
    SOURCES += singleapplication.cpp
    HEADERS += singleapplication.h
    QT      += network
    QMAKE_CXXFLAGS += -DSINGLE_APP=1
}

TRANSLATIONS    +=  localization/localization_nl_NL.ts \
                    localization/localization_de_DE.ts \
                    localization/localization_es_ES.ts \
                    localization/localization_gl_ES.ts \
                    localization/localization_hu_HU.ts \
                    localization/localization_sv_SE.ts \
                    localization/localization_pl_PL.ts \
                    localization/localization_ru_RU.ts

RESOURCES   += resources.qrc

win32 {
    RC_FILE = windows.rc
    static {
        QMAKE_LFLAGS += -static-libgcc -static-libstdc++
    }
    QMAKE_LFLAGS += -Wl,--dynamicbase -Wl,--nxcompat
    LIBS    += -lmpr
} else:macx {
    ICON = artwork/icon.icns
    QMAKE_INFO_PLIST = Info.plist
}

OTHER_FILES += LICENSE \
               README.md

target.path = /usr/local/bin/
INSTALLS    += target

DEFINES += "VERSION=\"\\\"$$VERSION\\\"\""
