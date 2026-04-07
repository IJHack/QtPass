#!/bin/bash
# Generate compile_commands.json for IDE/LSP tooling using bear
set -e

echo "Generating compile_commands.json..."

# Clean and regenerate Makefiles
make distclean || true
qmake6 -r

# Generate compile_commands.json using bear
bear -- make -j4

echo "Done. compile_commands.json generated."
