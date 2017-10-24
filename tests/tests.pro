!include(tests.pri) { error("Couldn't find the tests.pri file!") }

TEMPLATE = subdirs
CONFIG += no_docs_target
SUBDIRS += auto
exists(manual): SUBDIRS += manual
