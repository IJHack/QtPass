!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_userinfo.cpp

HEADERS   += ../../../src/userinfo.h

INCLUDEPATH += ../../../src

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX = 24
}
