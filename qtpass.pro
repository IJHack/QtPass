!include(qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

SUBDIRS += src

CONFIG(debug, debug|release) {
    SUBDIRS += tests
    tests.depends = src
}

OTHER_FILES += LICENSE \
               README.md \
               qtpass.1

RESOURCES += resources.qrc

# add Makefile target to generate code coverage
coverage.target = coverage
coverage.commands += cd src/debug && gcov "*.gcda" 1>/dev/null $$escape_expand(\\n\\t)
coverage.commands += $$escape_expand(\\n)

coverage.depends = check

# add Makefile target to generate code coverage using codecov
codecov.target = codecov
codecov.commands += cd src/ && gcov "*.cpp *.h debug/*.gcda" 1>/dev/null $$escape_expand(\\n\\t)
codecov.commands += codecov -X gcov $$escape_expand(\\n\\t)
codecov.commands += $$escape_expand(\\n)

codecov.depends = check

CONFIG(debug, debug|release) {
    QMAKE_EXTRA_TARGETS += coverage codecov
    QMAKE_CLEAN += 'src/debug/*.gc??' 'src/*.gcov'
}
