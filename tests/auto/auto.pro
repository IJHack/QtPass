TEMPLATE = subdirs
SUBDIRS += util ui model settings passwordconfig filecontent simpletransaction gpgkeystate exportpublickeydialog importkeydialog keygendialog trayicon configdialog locale mainwindow userinfo profileinit grepsearchcontroller passworddisplaypanel passworddialog passbackendfactory base32 totp imitatepass clipboardmanager usersdialog realpass processoutputpanel
win32: SUBDIRS -= executor
!win32: SUBDIRS += executor
!nosingleapp: SUBDIRS += singleapplication
!win32: SUBDIRS += integration
