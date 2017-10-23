!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = app
QT        += core gui

CONFIG += c++11
LIBS = -L"$$OUT_PWD/../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
INCLUDEPATH += ../src

macx {
    TARGET = QtPass
} else {
    TARGET = qtpass
}

SOURCES   += main.cpp

isEmpty(PREFIX) {
 PREFIX = $$(PREFIX)
}

isEmpty(PREFIX) {
 PREFIX = /usr/local
}
target.path = $$PREFIX/bin/

INSTALLS += target
