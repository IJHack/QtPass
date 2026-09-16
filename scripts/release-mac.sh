#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
set -euo pipefail

DOXYFILE_PATH="Doxyfile"
DOXYFILE_BACKUP=""
TMPFILE=""

cleanup() {
	local exit_code=$?
	rm -f "${TMPFILE:-}"
	if [[ $exit_code -ne 0 && -n "${DOXYFILE_BACKUP:-}" && -f "$DOXYFILE_BACKUP" ]]; then
		mv -f "$DOXYFILE_BACKUP" "$DOXYFILE_PATH"
	else
		rm -f "${DOXYFILE_BACKUP:-}"
	fi
}

trap cleanup EXIT

require_readable_file() {
	local file="$1"
	if [[ ! -r "$file" ]]; then
		echo "Error: $file is missing or not readable." >&2
		exit 1
	fi
}

echo "Extracting version..."
require_readable_file "qtpass.pri"
VERSION=$(awk -F= '/^[[:space:]]*VERSION[[:space:]]*=/ { gsub(/[[:space:]]/, "", $2); print $2; exit }' qtpass.pri)
if [[ -z "${VERSION:-}" ]]; then
	echo "Error: Failed to extract VERSION from qtpass.pri" >&2
	exit 1
fi
require_readable_file "$DOXYFILE_PATH"
# Doxygen doesn't expand $ENV{} in config, so substitute directly
# Use temp file for portable sed across GNU/BSD sed
if ! TMPFILE=$(mktemp); then
	echo "Error: Failed to create temporary working file." >&2
	exit 1
fi
if ! DOXYFILE_BACKUP=$(mktemp); then
	echo "Error: Failed to create temporary backup file." >&2
	rm -f "${TMPFILE:-}"
	exit 1
fi
if [[ -z "${TMPFILE:-}" || -z "${DOXYFILE_BACKUP:-}" ]]; then
	echo "Error: Temporary file path is empty." >&2
	rm -f "${TMPFILE:-}" "${DOXYFILE_BACKUP:-}"
	exit 1
fi
cp "$DOXYFILE_PATH" "$DOXYFILE_BACKUP"
if ! grep -qE '^PROJECT_NUMBER.*=.*' "$DOXYFILE_PATH"; then
	echo "Error: PROJECT_NUMBER entry not found in $DOXYFILE_PATH." >&2
	exit 1
fi
if ! sed "s|^PROJECT_NUMBER.*=.*|PROJECT_NUMBER         = $VERSION|" "$DOXYFILE_PATH" >"$TMPFILE"; then
	echo "Error: Failed to update PROJECT_NUMBER in $DOXYFILE_PATH." >&2
	exit 1
fi
if [[ ! -s "$TMPFILE" ]]; then
	echo "Error: Generated temporary Doxyfile is empty." >&2
	exit 1
fi
if ! grep -qF -- "PROJECT_NUMBER         = $VERSION" "$TMPFILE"; then
	echo "Error: PROJECT_NUMBER was not updated in $DOXYFILE_PATH." >&2
	exit 1
fi
if ! mv "$TMPFILE" "$DOXYFILE_PATH"; then
	echo "Error: Failed to replace $DOXYFILE_PATH with updated content." >&2
	exit 1
fi
echo "Generating API documentation (v$VERSION)..."
doxygen || {
	echo "Error: doxygen failed." >&2
	exit 1
}

echo "Running qmake6 (release)..."
if ! command -v qmake6 &>/dev/null; then
	echo "Error: qmake6 is not installed or not in PATH. Qt6 is required for building." >&2
	exit 1
fi
qmake6 CONFIG+=release || {
	echo "Error: qmake6 failed." >&2
	exit 1
}

echo "Running make..."
make || {
	echo "Error: make failed." >&2
	exit 1
}

echo "Running macdeployqt..."
if ! command -v macdeployqt &>/dev/null; then
	echo "Error: macdeployqt is not installed or not in PATH." >&2
	exit 1
fi
macdeployqt main/QtPass.app || {
	echo "Error: macdeployqt failed." >&2
	exit 1
}

echo "Creating DMG..."
# Same tool and layout as the release-installers workflow, so a local build
# and a CI build produce the same artifact.
if ! command -v create-dmg &>/dev/null; then
	echo "Error: create-dmg is not installed or not in PATH (brew install create-dmg)." >&2
	exit 1
fi
DMG_NAME="QtPass-${VERSION}.dmg"
rm -f "$DMG_NAME"
create-dmg "$DMG_NAME" main/QtPass.app || {
	echo "Error: create-dmg failed." >&2
	exit 1
}
echo "Created $DMG_NAME"
