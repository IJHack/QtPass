# Note: this assumes Qt with release and debug config was used

VERSION=v0.1-testing

qtpass-${VERSION}-win.zip: qtpass.exe LICENSE README.md
	7z a -mx=9 $@ $^

qtpass.exe: release/qtpass.exe
	strip $^
	if [ -n "$(SIGNKEY)" ] ; then osslsigncode sign -ts http://www.startssl.com/timestamp -certs $(SIGNKEY).crt -key $(SIGNKEY).key -in $^ -out $@ ; chmod +x $@ ; else cp $^ $@ ; fi
