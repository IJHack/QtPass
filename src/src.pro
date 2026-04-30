!include(../qtpass.pri) { error("Couldn't find the qtpass.pri file!") }

TEMPLATE   = lib
QT        += core gui
TARGET    = qtpass

CONFIG += c++17 staticlib
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT

TRANSLATIONS    +=  ../localization/localization_ar.ts \
                    ../localization/localization_ca.ts \
                    ../localization/localization_cs.ts \
                    ../localization/localization_da.ts \
                    ../localization/localization_de_DE.ts \
                    ../localization/localization_de_LU.ts \
                    ../localization/localization_el.ts \
                    ../localization/localization_en_GB.ts \
                    ../localization/localization_en_US.ts \
                    ../localization/localization_es_ES.ts \
                    ../localization/localization_es_MX.ts \
                    ../localization/localization_es_EC.ts \
                    ../localization/localization_es_AR.ts \
                    ../localization/localization_es_UY.ts \
                    ../localization/localization_et.ts \
                    ../localization/localization_fa.ts \
                    ../localization/localization_fr_BE.ts \
                    ../localization/localization_fr_FR.ts \
                    ../localization/localization_fr_LU.ts \
                    ../localization/localization_gl.ts \
                    ../localization/localization_he.ts \
                    ../localization/localization_hu.ts \
                    ../localization/localization_it.ts \
                    ../localization/localization_ko.ts \
                    ../localization/localization_lb_LU.ts \
                    ../localization/localization_nb.ts \
                    ../localization/localization_nl_BE.ts \
                    ../localization/localization_nl_NL.ts \
                    ../localization/localization_fy_NL.ts \
                    ../localization/localization_pl.ts \
                    ../localization/localization_pt_BR.ts \
                    ../localization/localization_pt_PT.ts \
                    ../localization/localization_ru.ts \
                    ../localization/localization_sk.ts \
                    ../localization/localization_sq.ts \
                    ../localization/localization_sv.ts \
                    ../localization/localization_tr.ts \
                    ../localization/localization_zh_CN.ts \
                    ../localization/localization_cy.ts \
                    ../localization/localization_hr.ts \
                    ../localization/localization_af.ts \
                    ../localization/localization_ro.ts \
                    ../localization/localization_si.ts \
                    ../localization/localization_sr_RS.ts \
                    ../localization/localization_sr_Cyrl.ts \
                    ../localization/localization_ja.ts \
                    ../localization/localization_bg.ts \
                    ../localization/localization_fi.ts \
                    ../localization/localization_uk.ts \
                    ../localization/localization_ta.ts \
                    ../localization/localization_id.ts \
                    ../localization/localization_lt.ts \
                    ../localization/localization_lv.ts \
                    ../localization/localization_sl.ts \
                    ../localization/localization_zh_Hant.ts \
                    ../localization/localization_th.ts \
                    ../localization/localization_sw.ts \
                    ../localization/localization_hi.ts \
                    ../localization/localization_bn.ts

CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX=/localization

SOURCES   += mainwindow.cpp \
             configdialog.cpp \
             storemodel.cpp \
             util.cpp \
             usersdialog.cpp \
             keygendialog.cpp \
             trayicon.cpp \
             passworddialog.cpp \
             exportpublickeydialog.cpp \
             importkeydialog.cpp \
             qprogressindicator.cpp \
             qpushbuttonwithclipboard.cpp \
             qpushbuttonasqrcode.cpp \
             qpushbuttonshowpassword.cpp \
             qtpasssettings.cpp \
             settingsconstants.cpp \
             pass.cpp \
             gpgkeystate.cpp \
             realpass.cpp \
             imitatepass.cpp \
             executor.cpp \
             simpletransaction.cpp \
             filecontent.cpp \
             qtpass.cpp \
             profileinit.cpp

HEADERS   += mainwindow.h \
             configdialog.h \
             storemodel.h \
             util.h \
             usersdialog.h \
             keygendialog.h \
             trayicon.h \
             passworddialog.h \
             exportpublickeydialog.h \
             importkeydialog.h \
             qprogressindicator.h \
             deselectabletreeview.h \
             qpushbuttonwithclipboard.h \
             qpushbuttonasqrcode.h \
             qpushbuttonshowpassword.h \
             qtpasssettings.h \
             enums.h \
             settingsconstants.h \
             pass.h \
             gpgkeystate.h \
             realpass.h \
             imitatepass.h \
             debughelper.h \
             executor.h \
             simpletransaction.h \
             filecontent.h \
             passwordconfiguration.h \
             userinfo.h \
             qtpass.h \
             profileinit.h

FORMS     += mainwindow.ui \
             configdialog.ui \
             usersdialog.ui \
             keygendialog.ui \
             passworddialog.ui \
             exportpublickeydialog.ui \
             importkeydialog.ui

!nosingleapp {
    SOURCES += singleapplication.cpp
    HEADERS += singleapplication.h
}

RESOURCES   += ../resources.qrc
