TEMPLATE = subdirs
SUBDIRS += util ui model settings passwordconfig filecontent simpletransaction gpgkeystate exportpublickeydialog importkeydialog keygendialog trayicon configdialog locale mainwindow userinfo profileinit grepsearchcontroller passworddisplaypanel passbackendfactory base32 totp
win32: SUBDIRS -= executor
!win32: SUBDIRS += executor
!win32: SUBDIRS += integration
