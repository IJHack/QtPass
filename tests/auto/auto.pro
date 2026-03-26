TEMPLATE = subdirs
SUBDIRS += util ui model settings passwordconfig filecontent simpletransaction
win32: SUBDIRS -= executor
!win32: SUBDIRS += executor
