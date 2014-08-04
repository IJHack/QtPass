#-------------------------------------------------
#
# Project created by QtCreator 2014-07-30T21:56:15
#
#-------------------------------------------------

QT        += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET     = qtpass
TEMPLATE   = app

SOURCES   += main.cpp\
             mainwindow.cpp \
             dialog.cpp

HEADERS   += mainwindow.h \
             dialog.h

FORMS     += mainwindow.ui \
             dialog.ui

TRANSLATIONS    +=  localization/localization_nl_NL.ts \
                    localization/localization_de_DE.ts \
                    localization/localization_hu_HU.ts

RESOURCES += resources.qrc

TRANSLATIONS += localization/localization_nl_NL.ts

RC_FILE = windows.rc

ICON += artwork/icon.svg \
        artwork/icon.icns
