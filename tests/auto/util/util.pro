!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

message($$QMAKE_LINK_OBJECT_MAX)

SOURCES += tst_util.cpp \

LIBS += -L../../../src -lqtpass

HEADERS   += util.h \
             qtpasssettings.h \
             settingsconstants.h \
             pass.h \
             realpass.h \
             imitatepass.h \
             executor.h \
             simpletransaction.h

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
    LIBS += -lbcrypt
	RC_FILE = ../../../windows.rc     
#	temporary workaround for QTBUG-6453
	QMAKE_LINK_OBJECT_MAX=24
#	setting this may also work, but I can't find appropriate value right now
#	QMAKE_LINK_OBJECT_SCRIPT = 
}
