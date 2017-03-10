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

# add Makefile target to generate code coverage using gcov
gcov.target = gcov
gcov.commands += cd src/debug && gcov "*.gcda" 1>/dev/null $$escape_expand(\\n\\t)
gcov.commands += $$escape_expand(\\n)
gcov.depends = check

# add Makefile target to generate code coverage using codecov
codecov.target = codecov
codecov.commands += cd src/ && gcov -o debug *.cpp *.h debug/*.gcda 1>/dev/null $$escape_expand(\\n\\t)
codecov.commands += codecov -X gcov $$escape_expand(\\n\\t)
codecov.commands += $$escape_expand(\\n)
codecov.depends = check

LCOV_OUTPUT_DIR = src/$$OBJECTS_DIR/lcov/
# add Makefile target to generate code coverage using lcov
lcov_initial.target = lcov_initial
#lcov_initial.commands =  $$escape_expand(\\n\\t)
lcov_initial.commands += rm -rf $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands +=  mkdir $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands += lcov --quiet --initial --capture --base-directory ./src --directory ./src/debug/ -o $${LCOV_OUTPUT_DIR}/.lcov.base1 $$escape_expand(\\n\\t)
lcov_initial.commands += $$escape_expand(\\n)
lcov_initial.depends += sub-src

lcov.target = lcov
lcov.commands += lcov -q -c -b ./src -d ./src/debug/ -o $${LCOV_OUTPUT_DIR}/.lcov.run1 $$escape_expand(\\n\\t)
lcov.commands += lcov -q -e $${LCOV_OUTPUT_DIR}/.lcov.base1 -o $${LCOV_OUTPUT_DIR}/.lcov.base $$PWD/src/* $$escape_expand(\\n\\t)
lcov.commands += lcov -q -e $${LCOV_OUTPUT_DIR}/.lcov.run1 -o $${LCOV_OUTPUT_DIR}/.lcov.run $$PWD/src/* $$escape_expand(\\n\\t)
lcov.commands += lcov -q -a $${LCOV_OUTPUT_DIR}/.lcov.base -a $${LCOV_OUTPUT_DIR}/.lcov.run -o $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov.commands += genhtml --demangle-cpp -o $${LCOV_OUTPUT_DIR}/ $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov.commands += @echo -e "xdg-open file:///$${PWD}/$${LCOV_OUTPUT_DIR}/index.html"
lcov.commands += $$escape_expand(\\n)
lcov.depends = lcov_initial check

CONFIG(debug, debug|release) {
    QMAKE_EXTRA_TARGETS += gcov codecov lcov lcov_initial
    QMAKE_CLEAN += 'src/debug/*.gc??' 'src/*.gcov'
}
