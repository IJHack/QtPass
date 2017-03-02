VERSION    = 1.2.0-pre

TEMPLATE = subdirs

CONFIG(debug, debug|release) {
	DESTDIR = debug 
	OBJECTS_DIR = debug 
	MOC_DIR = debug
	QMAKE_CXXFLAGS += --coverage
	QMAKE_LFLAGS += --coverage
	QMAKE_DISTCLEAN += -r $$OBJECTS_DIR
}

