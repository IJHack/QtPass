!include(tests.pri) { error("Couldn't find the tests.pri file!") }

CONFIG += no_docs_target
SUBDIRS += auto
exists(manual): SUBDIRS += manual
