VERSION    = 1.2.0-pre

TEMPLATE = subdirs

CONFIG(coverage) {
	QMAKE_LFLAGS += --coverage
}
