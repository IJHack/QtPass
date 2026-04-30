!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_locale.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

# Build and embed our own .qm files via lrelease so the test is independent
# of src/src.pro's build state and works on every platform (the auto-generated
# qmake_qmake_qm_files.qrc lives in src/'s build dir with absolute paths that
# don't translate cleanly to a different build root, e.g. on Windows CI).
TRANSLATIONS = $$files($$PWD/../../../localization/localization_*.ts)
CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX = /localization

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX = 24
}
