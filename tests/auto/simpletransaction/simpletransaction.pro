QT += testlib
QT -= gui

CONFIG += testcase
CONFIG -= app

TARGET = tst_simpletransaction

SOURCES += tst_simpletransaction.cpp \
    ../../../src/simpletransaction.cpp

HEADERS += ../../../src/simpletransaction.h \
    ../../../src/enums.h

INCLUDEPATH += ../../../src
