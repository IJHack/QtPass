!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_locale.cpp

TRANSLATIONS = $$files($$PWD/../../../localization/localization_*.ts)
CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX = /localization
