!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_util.cpp \
           util.cpp \
           settingsconstants.cpp \
           qtpasssettings.cpp

HEADERS   += qtpasssettings.h \
             settingsconstants.h \
             util.h

VPATH += ../../../src
INCLUDEPATH += ../../../src
