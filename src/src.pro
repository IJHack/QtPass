!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = app
QT        += core gui

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -g -c -Wall -O0
    QMAKE_LFLAGS += -O0
}

macx {
    TARGET = QtPass
    QMAKE_MAC_SDK = macosx
    QT += svg
} else {
    TARGET = qtpass
}

SOURCES   += main.cpp \
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
             executor.cpp \
             simpletransaction.cpp

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
             executor.h \
             simpletransaction.h

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


TRANSLATIONS    +=  ../localization/localization_nl_NL.ts \
                    ../localization/localization_de_DE.ts \
                    ../localization/localization_es_ES.ts \
                    ../localization/localization_gl_ES.ts \
                    ../localization/localization_hu_HU.ts \
                    ../localization/localization_sv_SE.ts \
                    ../localization/localization_pl_PL.ts \
                    ../localization/localization_ru_RU.ts \
                    ../localization/localization_he_IL.ts \
                    ../localization/localization_zh_CN.ts \
                    ../localization/localization_ar_MA.ts \
                    ../localization/localization_fr_FR.ts \
                    ../localization/localization_fr_BE.ts \
                    ../localization/localization_nl_BE.ts \
                    ../localization/localization_fr_LU.ts \
                    ../localization/localization_de_LU.ts \
                    ../localization/localization_lb_LU.ts \
                    ../localization/localization_en_GB.ts \
                    ../localization/localization_en_US.ts \
                    ../localization/localization_el_GR.ts \
                    ../localization/localization_cs_CZ.ts \
                    ../localization/localization_it_IT.ts \
                    ../localization/localization_pt_PT.ts

CODECFORSRC     = UTF-8
CODECFORTR      = UTF-8

isEmpty(QMAKE_LRELEASE) {
    win32|os2:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
        !exists($$QMAKE_LRELEASE) {
            greaterThan(QT_MAJOR_VERSION, 4) {
                QMAKE_LRELEASE = lrelease-qt5
            } else {
                QMAKE_LRELEASE = lrelease-qt4
            }
        }
    } else {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
    }
}

isEmpty(QMAKE_LUPDATE) {
    win32|os2:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]\\lupdate.exe
    else:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]/lupdate
    unix {
        !exists($$QMAKE_LUPDATE) {
            greaterThan(QT_MAJOR_VERSION, 4) {
                QMAKE_LUPDATE = lupdate-qt5
            } else {
                QMAKE_LUPDATE = lupdate-qt4
            }
        }
    } else {
        !exists($$QMAKE_LUPDATE) { QMAKE_LUPDATE = lupdate }
    }
}

updateqm.input = TRANSLATIONS
updateqm.output = ../localization/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ../localization/${QMAKE_FILE_BASE}.qm
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm
PRE_TARGETDEPS += compiler_updateqm_make_all

win32 {
        system($$QMAKE_LUPDATE ../qtpass.pro)
        system($$QMAKE_LRELEASE ../qtpass.pro)
} else {
        system($$QMAKE_LUPDATE $$_PRO_FILE_)
        system($$QMAKE_LRELEASE $$_PRO_FILE_)
}

RESOURCES   += ../resources.qrc

win32 {
    RC_FILE = ../windows.rc
    static {
        QMAKE_LFLAGS += -static-libgcc -static-libstdc++
    }
    gcc:QMAKE_LFLAGS += -Wl,--dynamicbase -Wl,--nxcompat
    msvc:QMAKE_LFLAGS += /DYNAMICBASE /NXCOMPAT
    LIBS    += -lmpr -lbcrypt
} else:macx {
    ICON = ../artwork/icon.icns
    QMAKE_INFO_PLIST = ../qtpass.plist
} else:bsd {
    LIBS += -L/usr/local/lib
}

isEmpty(PREFIX) {
 PREFIX = $$(PREFIX)
}

isEmpty(PREFIX) {
 PREFIX = /usr/local
}
target.path = $$PREFIX/bin/

INSTALLS += target
