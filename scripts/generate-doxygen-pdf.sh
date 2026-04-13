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

# Enable LaTeX and PDF output, and set the version (portable sed)
sed -i '' -e "s/^PROJECT_NUMBER.*=.*/PROJECT_NUMBER         = $VERSION/" \
	-e "s/^GENERATE_LATEX.*=.*/GENERATE_LATEX         = YES/" \
	-e "s/^GENERATE_PDFLATEX.*=.*/GENERATE_PDFLATEX     = YES/" \
	Doxyfile

echo "Generating LaTeX documentation..."
doxygen || {
	echo "Error: doxygen failed." >&2
	exit 1
}

# Find the generated makefile (Doxygen uses ./latex/ by default)
LATEX_MAKEFILE=$(find latex -name "Makefile" -type f 2>/dev/null | head -n1)
if [[ -z "$LATEX_MAKEFILE" ]]; then
	# Try docs/latex as fallback if OUTPUT_DIRECTORY was set to docs
	LATEX_MAKEFILE=$(find docs/latex -name "Makefile" -type f 2>/dev/null | head -n1)
fi
if [[ -z "$LATEX_MAKEFILE" ]]; then
	echo "Error: Could not find LaTeX Makefile (tried latex/ and docs/latex)" >&2
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

# Find the generated PDF and copy to accessible location
PDF_FILE=$(find latex -name "refman.pdf" -type f 2>/dev/null | head -n1)
if [[ -z "$PDF_FILE" ]]; then
	PDF_FILE=$(find docs/latex -name "refman.pdf" -type f 2>/dev/null | head -n1)
fi
if [[ -z "$PDF_FILE" ]]; then
	echo "Error: Could not find generated PDF (tried latex/ and docs/latex)" >&2
	exit 1
fi

# Determine output directory (same level as latex folder)
if [[ -d "docs" ]]; then
	OUTPUT_DIR="docs"
else
	OUTPUT_DIR="."
fi
cp "$PDF_FILE" "$OUTPUT_DIR/QtPass-$VERSION.pdf"
echo "PDF generated: $OUTPUT_DIR/QtPass-$VERSION.pdf"
