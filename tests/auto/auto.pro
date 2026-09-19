TEMPLATE = subdirs
SUBDIRS += util ui model settings passwordconfig filecontent simpletransaction gpgkeystate exportpublickeydialog importkeydialog keygendialog trayicon configdialog locale mainwindow userinfo profileinit grepsearchcontroller passworddisplaypanel passworddialog passbackendfactory base32 totp imitatepass clipboardmanager usersdialog realpass processoutputpanel storetree qtpass gpgidsigner firstrunwizard
# The executor suite guards its shell-script cases with Q_OS_WIN itself; the
# parser, resolver and gpgconf cases are what Windows is there to run.
SUBDIRS += executor
!nosingleapp: SUBDIRS += singleapplication
!win32: SUBDIRS += integration
