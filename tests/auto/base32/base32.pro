!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_base32.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

HEADERS   += base32.h

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX = 24
}
