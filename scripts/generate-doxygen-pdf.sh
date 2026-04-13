#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
set -euo pipefail

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
echo "Version: $VERSION"

require_readable_file "Doxyfile"

# Enable LaTeX and PDF output, and set the version
sed -i \
	-e "s/PROJECT_NUMBER.*=.*/PROJECT_NUMBER         = $VERSION/" \
	-e "s/GENERATE_LATEX.*=.*/GENERATE_LATEX         = YES/" \
	-e "s/GENERATE_PDFLATEX.*=.*/GENERATE_PDFLATEX     = YES/" \
	Doxyfile

echo "Generating LaTeX documentation..."
doxygen || {
	echo "Error: doxygen failed." >&2
	exit 1
}

# Find the generated makefile
LATEX_MAKEFILE=$(find docs/latex -name "Makefile" -type f | head -n1)
if [[ -z "$LATEX_MAKEFILE" ]]; then
	echo "Error: Could not find LaTeX Makefile in docs/latex" >&2
	exit 1
fi

LATEX_DIR=$(dirname "$LATEX_MAKEFILE")

echo "Building PDF (this may take a while)..."
cd "$LATEX_DIR"
make pdf || {
	echo "Error: LaTeX build failed." >&2
	exit 1
}
cd - >/dev/null

PDF_FILE=$(find "$LATEX_DIR" -name "refman.pdf" -type f | head -n1)
if [[ -z "$PDF_FILE" ]]; then
	echo "Error: Could not find generated PDF" >&2
	exit 1
fi

# Copy to more accessible location
cp "$PDF_FILE" "docs/QtPass-$VERSION.pdf"
echo "PDF generated: docs/QtPass-$VERSION.pdf"
