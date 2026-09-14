# QtPass - GUI for pass
# SPDX-FileCopyrightText: 2014 Anne Jan Brouwer

VERSION    = 1.8.0

CONFIG(coverage) {
	QMAKE_LFLAGS += --coverage
	QMAKE_CXXFLAGS += --coverage
}

CONFIG(debug, debug|release) {
    !msvc:QMAKE_CXXFLAGS += -g -c -Wall -O0
    QMAKE_LFLAGS += -O0
}

lessThan(QT_MAJOR_VERSION, 6): error("QtPass 2.x requires Qt 6.2 or newer (Qt 5 support ended with 1.8)")
equals(QT_MAJOR_VERSION, 6):lessThan(QT_MINOR_VERSION, 2): error("QtPass 2.x requires Qt 6.2 or newer")
QT += widgets

clang|gcc:QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas

nosingleapp {
    DEFINES += SINGLE_APP=0
} else {
    QT      += network
    DEFINES += SINGLE_APP=1
}

DEFINES += "VERSION=\"\\\"$$VERSION\\\"\""
# Enforce range-for over Qt's foreach/Q_FOREACH across src, main and tests.
DEFINES += QT_NO_FOREACH

CODECFORSRC     = UTF-8
CODECFORTR      = UTF-8

macx {
    QMAKE_MAC_SDK = macosx
    QT += svg
    CONFIG += app_bundle
}

isEmpty(QMAKE_LRELEASE) {
    win32|os2:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease-qt6 }
    } else {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
    }
}

isEmpty(QMAKE_LUPDATE) {
    win32|os2:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]\\lupdate.exe
    else:QMAKE_LUPDATE = $$[QT_INSTALL_BINS]/lupdate
    unix {
        !exists($$QMAKE_LUPDATE) { QMAKE_LUPDATE = lupdate-qt6 }
    } else {
        !exists($$QMAKE_LUPDATE) { QMAKE_LUPDATE = lupdate }
    }
}

winstore: DEFINES += "WINSTORE=1"

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
    QMAKE_INFO_PLIST = $$(PWD)/qtpass.plist
} else:bsd {
    LIBS += -L/usr/local/lib
}
