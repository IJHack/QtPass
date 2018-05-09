!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

CONFIG += testcase qt warn_on depend_includepath testcase no_testcase_installs
QT += testlib widgets

