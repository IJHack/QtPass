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

# Signing and notarization are opt-in, so an unsigned local build keeps working:
#   MAC_SIGN_IDENTITY   a "Developer ID Application: …" identity (name or SHA-1)
#                       from `security find-identity -v -p codesigning`
#   MAC_NOTARY_PROFILE  a profile saved with `xcrun notarytool store-credentials`;
#                       needs MAC_SIGN_IDENTITY, since Apple only notarizes signed code
# Both are checked here, before the build, so a typo doesn't cost a full build.
MAC_SIGN_IDENTITY="${MAC_SIGN_IDENTITY:-}"
MAC_NOTARY_PROFILE="${MAC_NOTARY_PROFILE:-}"
if [[ -n "$MAC_NOTARY_PROFILE" && -z "$MAC_SIGN_IDENTITY" ]]; then
	echo "Error: MAC_NOTARY_PROFILE is set but MAC_SIGN_IDENTITY is not; notarization needs a signed build." >&2
	exit 1
fi
if [[ -n "$MAC_SIGN_IDENTITY" ]]; then
	echo "Checking signing identity..."
	IDENTITY_LINE=$(security find-identity -v -p codesigning | grep -F -- "$MAC_SIGN_IDENTITY" | head -n 1 || true)
	if [[ -z "$IDENTITY_LINE" ]]; then
		echo "Error: no valid code-signing identity matches \"$MAC_SIGN_IDENTITY\"." >&2
		echo "       List them with: security find-identity -v -p codesigning" >&2
		exit 1
	fi
	if [[ -n "$MAC_NOTARY_PROFILE" && "$IDENTITY_LINE" != *"Developer ID Application:"* ]]; then
		echo "Error: notarization needs a \"Developer ID Application\" identity, got:" >&2
		echo "       $IDENTITY_LINE" >&2
		exit 1
	fi
fi
if [[ -n "$MAC_NOTARY_PROFILE" ]]; then
	echo "Checking notarization credentials..."
	if ! xcrun notarytool history --keychain-profile "$MAC_NOTARY_PROFILE" >/dev/null; then
		echo "Error: notarytool cannot use keychain profile \"$MAC_NOTARY_PROFILE\"." >&2
		echo "       Create it with: xcrun notarytool store-credentials $MAC_NOTARY_PROFILE" >&2
		exit 1
	fi
fi

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
MACDEPLOYQT_ARGS=()
if [[ -n "$MAC_SIGN_IDENTITY" ]]; then
	# Signs every bundled framework and plugin with the hardened runtime and a
	# secure timestamp, which notarization requires.
	MACDEPLOYQT_ARGS+=("-sign-for-notarization=$MAC_SIGN_IDENTITY")
fi
# The ${…+…} form keeps macOS's bash 3.2 from treating an empty array as unset.
macdeployqt main/QtPass.app ${MACDEPLOYQT_ARGS[@]+"${MACDEPLOYQT_ARGS[@]}"} || {
	echo "Error: macdeployqt failed." >&2
	exit 1
}
if [[ -n "$MAC_SIGN_IDENTITY" ]]; then
	echo "Verifying app signature..."
	codesign --verify --deep --strict --verbose=2 main/QtPass.app || {
		echo "Error: main/QtPass.app is not validly signed." >&2
		exit 1
	}
fi

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

if [[ -n "$MAC_SIGN_IDENTITY" ]]; then
	echo "Signing $DMG_NAME..."
	codesign --sign "$MAC_SIGN_IDENTITY" --timestamp "$DMG_NAME" || {
		echo "Error: signing $DMG_NAME failed." >&2
		exit 1
	}
fi

if [[ -n "$MAC_NOTARY_PROFILE" ]]; then
	echo "Submitting $DMG_NAME for notarization (usually a few minutes)..."
	# `submit --wait` can exit 0 for a rejected submission, so read the status.
	NOTARY_JSON=$(xcrun notarytool submit "$DMG_NAME" --keychain-profile "$MAC_NOTARY_PROFILE" \
		--wait --output-format json) || {
		echo "Error: notarytool submit failed." >&2
		exit 1
	}
	NOTARY_STATUS=$(plutil -extract status raw -o - - <<<"$NOTARY_JSON" || true)
	if [[ "$NOTARY_STATUS" != "Accepted" ]]; then
		NOTARY_ID=$(plutil -extract id raw -o - - <<<"$NOTARY_JSON" || true)
		echo "Error: notarization status is \"${NOTARY_STATUS:-unknown}\"." >&2
		echo "       See why with: xcrun notarytool log ${NOTARY_ID:-<id>} --keychain-profile $MAC_NOTARY_PROFILE" >&2
		exit 1
	fi
	echo "Stapling the notarization ticket..."
	xcrun stapler staple "$DMG_NAME" || {
		echo "Error: stapling $DMG_NAME failed." >&2
		exit 1
	}
	spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG_NAME" || {
		echo "Error: Gatekeeper rejects $DMG_NAME." >&2
		exit 1
	}
fi

if [[ -n "$MAC_NOTARY_PROFILE" ]]; then
	echo "Created $DMG_NAME (signed and notarized)"
elif [[ -n "$MAC_SIGN_IDENTITY" ]]; then
	echo "Created $DMG_NAME (signed, not notarized)"
else
	echo "Created $DMG_NAME (unsigned)"
fi
