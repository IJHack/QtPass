#!/usr/bin/env bash
set -euo pipefail

(
	cd artwork
	cp icon.png qtpass-icon.png
	xdg-icon-resource install --size 64 qtpass-icon.png
)

echo "Running qmake (release)..."
qmake CONFIG+=release || {
	echo "Error: qmake failed."
	exit 1
}

echo "Running make..."
make || {
	echo "Error: make failed."
	exit 1
}

echo "Running make install..."
sudo make install || {
	echo "Error: make install failed."
	exit 1
}

sudo cp qtpass.desktop /usr/share/applications/
