TEMPLATE = subdirs

SUBDIRS += src

CONFIG(debug, debug|release) {
    SUBDIRS += tests
}

OTHER_FILES += LICENSE \
               README.md \
               qtpass.1

RESOURCES += resources.qrc

