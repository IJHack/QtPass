TEMPLATE = subdirs

SUBDIRS += src

CONFIG(debug, debug|release) {
    SUBDIRS += tests
}

OTHER_FILES += LICENSE \
               README.md \
               qtpass.1

isEmpty(PREFIX) {
 PREFIX = $$(PREFIX)
}

isEmpty(PREFIX) {
 PREFIX = /usr/local
}
target.path = $$PREFIX/bin/

INSTALLS += target

DISTFILES += \
    settingsToDelete.txt

