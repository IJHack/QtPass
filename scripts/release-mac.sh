#!/usr/bin/env bash
set -euo pipefail

README_CLEAN="README.clean"

cleanup() {
	rm -f "$README_CLEAN"
}

trap cleanup EXIT

if [[ ! -r README.md ]]; then
	echo "Error: README.md is missing or not readable." >&2
	exit 1
fi

if [[ ! -r FAQ.md ]]; then
	echo "Error: FAQ.md is missing or not readable." >&2
	exit 1
fi
if [[ ! -r CONTRIBUTING.md ]]; then
	echo "Error: CONTRIBUTING.md is missing or not readable." >&2
	exit 1
fi
if [[ ! -r CHANGELOG.md ]]; then
	echo "Error: CHANGELOG.md is missing or not readable." >&2
	exit 1
fi

echo "Processing README links..."
sed \
	-e 's/FAQ\.md/https:\/\/qtpass.org\/docs\/md__f_a_q.html/' \
	-e 's/CONTRIBUTING\.md/https:\/\/qtpass.org\/docs\/md__c_o_n_t_r_i_b_u_t_i_n_g.html/' \
	-e 's/CHANGELOG\.md/https:\/\/qtpass.org\/docs\/md__c_h_a_n_g_e_l_o_g.html/' \
	-e 's/\[\!.*//' \
	<README.md >"$README_CLEAN"

echo "Generating RTF documentation..."
pandoc --standalone --from=gfm --to=rtf --output=README.rtf "$README_CLEAN" FAQ.md CONTRIBUTING.md CHANGELOG.md || {
	echo "Error: pandoc failed while generating README.rtf from '$README_CLEAN', FAQ.md, CONTRIBUTING.md, and CHANGELOG.md. Check that input files exist and that pandoc is installed and available in PATH." >&2
	exit 1
}

echo "Generating API documentation..."
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
macdeployqt main/QtPass.app || {
	echo "Error: macdeployqt failed." >&2
	exit 1
}

echo "Creating DMG..."
appdmg appdmg.json main/QtPass.dmg || {
	echo "Error: appdmg failed." >&2
	exit 1
}
