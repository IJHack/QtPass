# QtPass - GUI for pass
# SPDX-FileCopyrightText: 2014 Anne Jan Brouwer

!include(qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE = subdirs
SUBDIRS += src tests main
main.depends = src
# The suites link libqtpass.a from src/; nothing in them needs main/.
tests.depends = src

OTHER_FILES += LICENSE \
               README.md \
               qtpass.1

# add Makefile target to generate code coverage using gcov
gcov.target = gcov
gcov.commands += cd src/$$OBJECTS_DIR && gcov "*.gcda" 1>/dev/null $$escape_expand(\\n\\t)
gcov.commands += $$escape_expand(\\n)
gcov.depends = check

# add Makefile target to generate code coverage using codecov
codecov.target = codecov
codecov.commands += cd src/ && codecov do-upload $$escape_expand(\\n\\t)
codecov.commands += $$escape_expand(\\n)
codecov.depends = check

LCOV_OUTPUT_DIR = docs/coverage
# add Makefile target to generate code coverage using lcov
lcov_initial.target = lcov_initial
#lcov_initial.commands =  $$escape_expand(\\n\\t)
lcov_initial.commands += rm -rf $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands +=  mkdir -p $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands += lcov --quiet --initial --capture --ignore-errors inconsistent --base-directory ./src --directory ./src/$$OBJECTS_DIR/ -o $${LCOV_OUTPUT_DIR}/.lcov.base1 $$escape_expand(\\n\\t)
lcov_initial.commands += $$escape_expand(\\n)
lcov_initial.depends += sub-src

lcov_prepare.target = lcov_prepare
lcov_prepare.commands += lcov -q --ignore-errors unused,inconsistent -c -b ./src -d ./src/$$OBJECTS_DIR -o $${LCOV_OUTPUT_DIR}/.lcov.run1 $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q --ignore-errors unused,inconsistent -e $${LCOV_OUTPUT_DIR}/.lcov.base1 -o $${LCOV_OUTPUT_DIR}/.lcov.base $$PWD/src/* $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q --ignore-errors unused,inconsistent -e $${LCOV_OUTPUT_DIR}/.lcov.run1 -o $${LCOV_OUTPUT_DIR}/.lcov.run $$PWD/src/* $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q --ignore-errors inconsistent -a $${LCOV_OUTPUT_DIR}/.lcov.base -a $${LCOV_OUTPUT_DIR}/.lcov.run -o $${LCOV_OUTPUT_DIR}/.lcov.all $$escape_expand(\\n\\t)
# Leave out generated code: moc, rcc and uic output, which an in-source build
# (as in CI) writes into src/, and the .moc files some sources include.
lcov_prepare.commands += lcov -q --ignore-errors unused,inconsistent -r $${LCOV_OUTPUT_DIR}/.lcov.all \"*/moc_*\" \"*/qrc_*\" \"*/ui_*\" \"*.moc\" -o $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov_prepare.commands += $$escape_expand(\\n)
lcov_prepare.depends = lcov_initial check

lcov.target = lcov
# One flat table of files, with the qtpass.org look: the report is published
# at qtpass.org/docs/coverage/. The prolog starts every page (HTML5, a
# viewport, a link back to the developer documentation); genhtml fills in its
# @pagetitle@ and @basedir@ and closes the page itself, hence the .in suffix.
lcov.commands += genhtml --demangle-cpp --flat --prefix $$PWD --title QtPass --header-title \"QtPass test coverage\" --html-prolog $$PWD/doxygen/coverage-prolog.html.in --css-file $$PWD/doxygen/coverage.css -o $${LCOV_OUTPUT_DIR}/ $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov.commands += @echo -e "xdg-open file:///$${PWD}/$${LCOV_OUTPUT_DIR}/index.html"
lcov.commands += $$escape_expand(\\n)
lcov.depends = lcov_prepare

coveralls.target = coveralls
coveralls.commands += cat $${LCOV_OUTPUT_DIR}/.lcov.total | coveralls $$escape_expand(\\n\\t)
coveralls.commands += $$escape_expand(\\n)
coveralls.depends = lcov_prepare

CONFIG(coverage) {
    QMAKE_EXTRA_TARGETS += gcov codecov lcov_initial lcov_prepare lcov coveralls
    QMAKE_CLEAN += src/$$OBJECTS_DIR/*.gc?? src/*.gcov docs/coverage/*
	QMAKE_DISTCLEAN += -r docs/coverage/
}

# Refreshing the translation sources is an explicit target, not something every
# qmake run does: lupdate rewrites all 64 .ts files (line numbers included), so
# a plain configure used to leave the whole localization/ directory dirty and
# CI checkouts modified. Run it when you add or change a tr() string:
#
#   make lupdate
#
lupdate.target = lupdate
lupdate.commands = $$QMAKE_LUPDATE -locations absolute $$PWD/src $$PWD/main -ts $$files($$PWD/localization/*.ts)
lupdate.depends = FORCE
QMAKE_EXTRA_TARGETS += lupdate
