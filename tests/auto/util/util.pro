!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

message($$QMAKE_LINK_OBJECT_MAX)

SOURCES += tst_util.cpp \

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS

HEADERS   += util.h \
             filecontent.h

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
	RC_FILE = ../../../windows.rc     
#	temporary workaround for QTBUG-6453
	QMAKE_LINK_OBJECT_MAX=24
#	setting this may also work, but I can't find appropriate value right now
#	QMAKE_LINK_OBJECT_SCRIPT = 
}
