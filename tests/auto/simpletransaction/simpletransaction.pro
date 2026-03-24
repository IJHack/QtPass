!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

TARGET = tst_simpletransaction

SOURCES += tst_simpletransaction.cpp \
    ../../../src/simpletransaction.cpp

HEADERS += ../../../src/simpletransaction.h \
    ../../../src/enums.h

INCLUDEPATH += ../../../src
