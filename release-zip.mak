# Note: this assumes Qt with release and debug config was used

VERSION=v1.0

qtpass-${VERSION}-gpg4win.zip: qtpass.exe qtpass.ini LICENSE README.md password-store gpg4win key_management.bat
	7z a -mx=9 $@ $^

qtpass.exe: release/qtpass.exe
	strip $^
	if [ -n "$(SIGNKEY)" ] ; then osslsigncode sign -ts http://www.startssl.com/timestamp -certs $(SIGNKEY).crt -key $(SIGNKEY).key -in $^ -out $@ ; chmod +x $@ ; else cp $^ $@ ; fi
