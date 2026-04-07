#!/usr/bin/env bash
set -euo pipefail

(
	cd artwork
	cp icon.png qtpass-icon.png
	xdg-icon-resource install --size 64 qtpass-icon.png
)

qmake CONFIG+=release && make && sudo make install
sudo cp qtpass.desktop /usr/share/applications/
