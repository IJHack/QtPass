!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

CONFIG += testcase qt warn_on depend_includepath testcase
QT += testlib widgets

target.path = $$[QT_INSTALL_TESTS]/qtpass/$$TARGET
INSTALLS += target
