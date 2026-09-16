!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = app
QT        += core gui

CONFIG += c++17
LIBS = -L"$$OUT_PWD/../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../src/$(OBJECTS_DIR)/libqtpass.a"

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

desktop.path = $$PREFIX/share/applications
desktop.files = ../qtpass.desktop
metainfo.path = $$PREFIX/share/metainfo
metainfo.files = ../qtpass.appdata.xml
icon_scalable.path = $$PREFIX/share/icons/hicolor/scalable/apps
icon_scalable.files = ../artwork/qtpass-icon.svg
icon_512.path = $$PREFIX/share/icons/hicolor/512x512/apps
# installed under the desktop file's Icon= name; the source keeps its old name
icon_512.extra = $(INSTALL_FILE) $$shell_quote($$PWD/../artwork/icon.png) $$shell_quote($(INSTALL_ROOT)$$PREFIX/share/icons/hicolor/512x512/apps/qtpass-icon.png)
manpage.path = $$PREFIX/share/man/man1
manpage.files = ../qtpass.1

INSTALLS += target desktop metainfo icon_scalable icon_512 manpage
