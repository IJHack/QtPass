!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = lib
QT        += core gui
TARGET 	   = qtpass

CONFIG += c++11 staticlib
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

TRANSLATIONS    +=  ../localization/localization_nl_NL.ts \
                    ../localization/localization_ca.ts \
                    ../localization/localization_de_DE.ts \
                    ../localization/localization_es_ES.ts \
                    ../localization/localization_gl_ES.ts \
                    ../localization/localization_hu_HU.ts \
                    ../localization/localization_sv_SE.ts \
                    ../localization/localization_pl_PL.ts \
                    ../localization/localization_ru_RU.ts \
                    ../localization/localization_he_IL.ts \
                    ../localization/localization_zh_CN.ts \
                    ../localization/localization_ar_MA.ts \
                    ../localization/localization_fr_FR.ts \
                    ../localization/localization_fr_BE.ts \
                    ../localization/localization_nl_BE.ts \
                    ../localization/localization_fr_LU.ts \
                    ../localization/localization_de_LU.ts \
                    ../localization/localization_lb_LU.ts \
                    ../localization/localization_en_GB.ts \
                    ../localization/localization_en_US.ts \
                    ../localization/localization_el_GR.ts \
                    ../localization/localization_cs_CZ.ts \
                    ../localization/localization_it_IT.ts \
                    ../localization/localization_sq_AL.ts \
                    ../localization/localization_pt_PT.ts \
                    ../localization/localization_nb_NO.ts

SOURCES   += mainwindow.cpp \
             configdialog.cpp \
             storemodel.cpp \
             util.cpp \
             usersdialog.cpp \
             keygendialog.cpp \
             trayicon.cpp \
             passworddialog.cpp \
             qprogressindicator.cpp \
             qpushbuttonwithclipboard.cpp \
             qpushbuttonasqrcode.cpp \
             qtpasssettings.cpp \
             settingsconstants.cpp \
             pass.cpp \
             realpass.cpp \
             imitatepass.cpp \
             executor.cpp \
             simpletransaction.cpp \
             filecontent.cpp \
   	     qtpass.cpp

HEADERS   += mainwindow.h \
             configdialog.h \
             storemodel.h \
             util.h \
             usersdialog.h \
             keygendialog.h \
             trayicon.h \
             passworddialog.h \
             qprogressindicator.h \
             deselectabletreeview.h \
             qpushbuttonwithclipboard.h \
             qpushbuttonasqrcode.h \
             qtpasssettings.h \
             enums.h \
             settingsconstants.h \
             pass.h \
             realpass.h \
             imitatepass.h \
             debughelper.h \
             executor.h \
             simpletransaction.h \
             filecontent.h \
             passwordconfiguration.h \
             userinfo.h \
             qtpass.h

FORMS     += mainwindow.ui \
             configdialog.ui \
             usersdialog.ui \
             keygendialog.ui \
             passworddialog.ui

updateqm.input = TRANSLATIONS
updateqm.output = ../localization/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ../localization/${QMAKE_FILE_BASE}.qm
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm

!nosingleapp {
    SOURCES += singleapplication.cpp
    HEADERS += singleapplication.h
}

RESOURCES   += ../resources.qrc
