VERSION    = 1.2.3

CONFIG(coverage) {
	QMAKE_LFLAGS += --coverage
	QMAKE_CXXFLAGS += --coverage
}

CONFIG(debug, debug|release) {
    !msvc:QMAKE_CXXFLAGS += -g -c -Wall -O0
    QMAKE_LFLAGS += -O0
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

clang|gcc:QMAKE_CXXFLAGS_WARN_ON += -Wno-unknown-pragmas

nosingleapp {
    QMAKE_CXXFLAGS += -DSINGLE_APP=0
} else {
    QT      += network
    QMAKE_CXXFLAGS += -DSINGLE_APP=1
}

DEFINES += "VERSION=\"\\\"$$VERSION\\\"\""

CODECFORSRC     = UTF-8
CODECFORTR      = UTF-8

macx {
    QMAKE_MAC_SDK = macosx
    QT += svg
}

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

