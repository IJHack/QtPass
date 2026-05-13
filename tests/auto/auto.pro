TEMPLATE = subdirs
SUBDIRS += util ui model settings passwordconfig filecontent simpletransaction gpgkeystate exportpublickeydialog importkeydialog keygendialog trayicon configdialog locale
win32: SUBDIRS -= executor
!win32: SUBDIRS += executor
!win32: SUBDIRS += integration
