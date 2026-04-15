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

# Escape VERSION for sed to avoid injection
ESCAPED_VERSION=$(printf '%s' "$VERSION" | sed -e 's/[\\&|]/\\&/g')

require_readable_file "Doxyfile"

# Enable LaTeX and PDF output, and set the version (portable sed with temp file)
DOXYFILE="$(pwd)/Doxyfile"
if ! DOXYFILE_BACKUP=$(mktemp); then
	echo "Error: Failed to create temporary backup file." >&2
	exit 1
fi
if ! TMPFILE=$(mktemp); then
	echo "Error: Failed to create temporary working file." >&2
	rm -f "${DOXYFILE_BACKUP:-}"
	exit 1
fi
if [[ -z "${DOXYFILE_BACKUP:-}" || -z "${TMPFILE:-}" ]]; then
	echo "Error: Temporary file path is empty." >&2
	rm -f "${DOXYFILE_BACKUP:-}" "${TMPFILE:-}"
	exit 1
fi
cp Doxyfile "$DOXYFILE_BACKUP"
cleanup_doxyfile_on_exit() {
	local status=$?
	if [[ $status -ne 0 ]]; then
		cp "$DOXYFILE_BACKUP" "$DOXYFILE"
		rm -f "$DOXYFILE_BACKUP"
	else
		rm -f "$DOXYFILE_BACKUP"
	fi
	rm -f "$TMPFILE"
}
trap cleanup_doxyfile_on_exit EXIT
sed -e "s|^PROJECT_NUMBER.*=.*|PROJECT_NUMBER         = $ESCAPED_VERSION|" \
	-e "s/^GENERATE_LATEX.*=.*/GENERATE_LATEX         = YES/" \
	-e "s/^USE_PDFLATEX.*=.*/USE_PDFLATEX           = YES/" \
	Doxyfile >"$TMPFILE" && mv "$TMPFILE" Doxyfile

echo "Generating LaTeX documentation..."
doxygen || {
	echo "Error: doxygen failed." >&2
	exit 1
}

# Search paths for generated LaTeX artifacts (primary + fallback)
LATEX_SEARCH_DIRS=("latex" "docs/latex")

# Find the generated makefile (Doxygen uses ./latex/ by default)
LATEX_MAKEFILE=""
for dir in "${LATEX_SEARCH_DIRS[@]}"; do
	LATEX_MAKEFILE=$(find "$dir" -name "Makefile" -type f 2>/dev/null | head -n1)
	if [[ -n "$LATEX_MAKEFILE" ]]; then
		break
	fi
done
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
# Check the actual build directory first
PDF_FILE=$(find "$LATEX_DIR" -name "refman.pdf" -type f 2>/dev/null | head -n1)
if [[ -z "$PDF_FILE" ]]; then
	# Fall back to search paths
	for dir in "${LATEX_SEARCH_DIRS[@]}"; do
		PDF_FILE=$(find "$dir" -name "refman.pdf" -type f 2>/dev/null | head -n1)
		if [[ -n "$PDF_FILE" ]]; then
			break
		fi
	done
fi
if [[ -z "$PDF_FILE" ]]; then
	echo "Error: Could not find generated PDF (tried latex/ and docs/latex)" >&2
	exit 1
fi

# Derive output directory from PDF_FILE location
OUTPUT_DIR=$(dirname "$PDF_FILE")
cp "$PDF_FILE" "$OUTPUT_DIR/QtPass-$VERSION.pdf"
echo "PDF generated: $OUTPUT_DIR/QtPass-$VERSION.pdf"
