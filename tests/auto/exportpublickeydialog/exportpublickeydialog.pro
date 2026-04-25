!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_exportpublickeydialog.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
    RC_FILE = ../../../windows.rc
    # Keep linker command size manageable on Windows toolchains.
    # This limit avoids command-line/linker object count constraints.
    QMAKE_LINK_OBJECT_MAX=24
}
