!include(../tests.pri) { error("Couldn't find the tests.pri file!") }

TEMPLATE = app

!contains(TARGET, ^tst_.*):TARGET = $$join(TARGET,,"tst_")

# Shared test helpers (header-only)
HEADERS += $$PWD/testsettings.h

# Every suite links the static library built in src/ and finds its headers
# there. The library already contains the moc output for its own classes, so
# a suite lists only its own tst_*.cpp; re-listing library headers here would
# moc them a second time into the test binary.
INCLUDEPATH += $$PWD/../../src
LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

win32 {
    RC_FILE = $$PWD/../../windows.rc
    # Keep the linker command size below Windows toolchain limits.
    QMAKE_LINK_OBJECT_MAX = 24
}
