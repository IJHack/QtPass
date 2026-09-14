#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Derive a CI manifest that builds the current checkout instead of the
# tagged release the Flathub manifest pins. Keeps every other module as-is.
set -euo pipefail
cd "$(dirname "$0")"

out=org.qtpass.QtPass.ci.yml
sed '/^  - name: qtpass$/,$ { /^    sources:$/,$d }' org.qtpass.QtPass.yml >"$out"
printf '%s\n' \
	'    sources:' \
	'      - type: dir' \
	'        path: ..' \
	'        skip:' \
	'          - .git' >>"$out"
echo "wrote $(pwd)/$out"
