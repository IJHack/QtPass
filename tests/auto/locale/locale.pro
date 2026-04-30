!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_locale.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

# Pull in the embedded .qm resource that src.pro generates so the test can
# load translations the same way the running app does.
RESOURCES += ../../../src/qmake_qmake_qm_files.qrc

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX = 24
}
