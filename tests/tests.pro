QT += widgets testlib
CONFIG += no_docs_target
TEMPLATE = subdirs
SUBDIRS += auto
exists(manual): SUBDIRS += manual
