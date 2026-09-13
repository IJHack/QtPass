!include(../tests.pri) { error("Couldn't find the tests.pri file!") }

TEMPLATE = app

!contains(TARGET, ^tst_.*):TARGET = $$join(TARGET,,"tst_")

# Shared test helpers (header-only)
HEADERS += $$PWD/testsettings.h
