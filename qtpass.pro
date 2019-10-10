!include(qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE = subdirs
SUBDIRS += src tests main
main.depends = src
tests.depends = main

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
codecov.commands += cd src/ && codecov $$escape_expand(\\n\\t)
codecov.commands += $$escape_expand(\\n)
codecov.depends = check

LCOV_OUTPUT_DIR = src/$$OBJECTS_DIR/lcov/
# add Makefile target to generate code coverage using lcov
lcov_initial.target = lcov_initial
#lcov_initial.commands =  $$escape_expand(\\n\\t)
lcov_initial.commands += rm -rf $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands +=  mkdir $$LCOV_OUTPUT_DIR $$escape_expand(\\n\\t)
lcov_initial.commands += lcov --quiet --initial --capture --base-directory ./src --directory ./src/$$OBJECTS_DIR/ -o $${LCOV_OUTPUT_DIR}/.lcov.base1 $$escape_expand(\\n\\t)
lcov_initial.commands += $$escape_expand(\\n)
lcov_initial.depends += sub-src

lcov_prepare.target = lcov_prepare
lcov_prepare.commands += lcov -q -c -b ./src -d ./src/$$OBJECTS_DIR -o $${LCOV_OUTPUT_DIR}/.lcov.run1 $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q -e $${LCOV_OUTPUT_DIR}/.lcov.base1 -o $${LCOV_OUTPUT_DIR}/.lcov.base $$PWD/src/* $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q -e $${LCOV_OUTPUT_DIR}/.lcov.run1 -o $${LCOV_OUTPUT_DIR}/.lcov.run $$PWD/src/* $$escape_expand(\\n\\t)
lcov_prepare.commands += lcov -q -a $${LCOV_OUTPUT_DIR}/.lcov.base -a $${LCOV_OUTPUT_DIR}/.lcov.run -o $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov_prepare.commands += $$escape_expand(\\n)
lcov_prepare.depends = lcov_initial check

lcov.target = lcov
lcov.commands += genhtml --demangle-cpp -o $${LCOV_OUTPUT_DIR}/ $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
lcov.commands += @echo -e "xdg-open file:///$${PWD}/$${LCOV_OUTPUT_DIR}/index.html"
lcov.commands += $$escape_expand(\\n)
lcov.depends = lcov_prepare

coveralls.target = coveralls
coveralls.commands += coveralls-lcov $${LCOV_OUTPUT_DIR}/.lcov.total $$escape_expand(\\n\\t)
coveralls.commands += $$escape_expand(\\n)
coveralls.depends = lcov_prepare

CONFIG(coverage) {
    QMAKE_EXTRA_TARGETS += gcov codecov lcov_initial lcov_prepare lcov coveralls
    QMAKE_CLEAN += src/$$OBJECTS_DIR/*.gc?? src/*.gcov
	QMAKE_DISTCLEAN += -r src/$$OBJECTS_DIR/lcov/
}

TRANSLATIONS    +=  localization/localization_ca.ts \
                    localization/localization_nl_NL.ts \
                    localization/localization_nl_BE.ts \
                    localization/localization_de_DE.ts \
                    localization/localization_es_ES.ts \
                    localization/localization_gl_ES.ts \
                    localization/localization_hu_HU.ts \
                    localization/localization_sv_SE.ts \
                    localization/localization_pl_PL.ts \
                    localization/localization_ru_RU.ts \
                    localization/localization_he_IL.ts \
                    localization/localization_zh_CN.ts \
                    localization/localization_ar_MA.ts \
                    localization/localization_fr_FR.ts \
                    localization/localization_fr_BE.ts \
                    localization/localization_fr_LU.ts \
                    localization/localization_de_LU.ts \
                    localization/localization_lb_LU.ts \
                    localization/localization_en_GB.ts \
                    localization/localization_en_US.ts \
                    localization/localization_el_GR.ts \
                    localization/localization_cs_CZ.ts \
                    localization/localization_it_IT.ts \
                    localization/localization_pt_PT.ts

system($$QMAKE_LUPDATE -locations none -no-ui-lines ./src ./main -ts localization/*.ts)
