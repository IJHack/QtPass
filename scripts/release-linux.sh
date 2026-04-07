qmake CONFIG+=release && make && sudo make install
cd artwork
cp icon.png qtpass-icon.png
xdg-icon-resource install --size 64 qtpass-icon.png 
cd ..
sudo cp qtpass.desktop /usr/share/applications/
