#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "artwork" ]; then
	echo "Error: required directory 'artwork' does not exist." >&2
	exit 1
fi

(
	cd artwork
	cp icon.png qtpass-icon.png || {
		echo "Error: failed to copy icon.png" >&2
		exit 1
	}
	xdg-icon-resource install --size 64 qtpass-icon.png || {
		echo "Error: icon installation failed." >&2
		exit 1
	}
)

echo "Running qmake (release)..."
qmake CONFIG+=release || {
	echo "Error: qmake failed." >&2
	exit 1
}

echo "Running make..."
make || {
	echo "Error: make failed." >&2
	exit 1
}

echo "Running make install..."
sudo make install || {
	echo "Error: make install failed." >&2
	exit 1
}

if [ ! -f qtpass.desktop ]; then
	echo "Error: qtpass.desktop not found." >&2
	exit 1
fi

sudo cp qtpass.desktop /usr/share/applications/ || {
	echo "Error: failed to copy qtpass.desktop to /usr/share/applications/." >&2
	exit 1
}
