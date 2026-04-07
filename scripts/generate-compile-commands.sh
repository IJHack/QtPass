#!/bin/bash
# Generate compile_commands.json for IDE/LSP tooling using bear
set -euo pipefail

# Check dependencies
command -v bear >/dev/null 2>&1 || {
	echo "Error: bear is not installed. Please install it first." >&2
	exit 1
}
command -v qmake6 >/dev/null 2>&1 || {
	echo "Error: qmake6 is not installed. Please install Qt development tools." >&2
	exit 1
}

# Determine parallelism based on available CPUs
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
MAKE_JOBS=$((NPROC > 0 ? NPROC : 4))

echo "Generating compile_commands.json..."

# Clean and regenerate Makefiles
make distclean || true
qmake6 -r

# Generate compile_commands.json using bear
bear -- make -j"${MAKE_JOBS}"

echo "Done. compile_commands.json generated."
