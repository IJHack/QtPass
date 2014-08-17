#-------------------------------------------------
#
# Project created by QtCreator 2014-07-30T21:56:15
#
#-------------------------------------------------

QT        += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

macx {
    TARGET = QtPass
} else {
    TARGET = qtpass
}

TEMPLATE   = app

SOURCES   += main.cpp\
             mainwindow.cpp \
             dialog.cpp \
             storemodel.cpp \
             singleapplication.cpp \
    util.cpp

HEADERS   += mainwindow.h \
             dialog.h \
             storemodel.h \
             singleapplication.h \
    util.h

FORMS     += mainwindow.ui \
             dialog.ui

TRANSLATIONS    +=  localization/localization_nl_NL.ts \
                    localization/localization_de_DE.ts \
                    localization/localization_hu_HU.ts \
                    localization/localization_sv_SE.ts \
                    localization/localization_pl_PL.ts

RESOURCES += resources.qrc

win32 {
    RC_FILE = windows.rc
} else:macx {
    ICON = artwork/icon.icns
}

OTHER_FILES += LICENSE \
               README.md

target.path = /usr/local/bin/
INSTALLS += target
