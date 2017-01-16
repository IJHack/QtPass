!include(../tests.pri) { error("Couldn't find the tests.pri file!") }

!contains(TARGET, ^tst_.*):TARGET = $$join(TARGET,,"tst_")
