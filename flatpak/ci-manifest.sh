#!/bin/sh
# Derive a CI manifest that builds the current checkout instead of the
# tagged release the Flathub manifest pins. Keeps every other module as-is.
set -eu
cd "$(dirname "$0")"
sed '/^  - name: qtpass$/,$ { /^    sources:$/,$d }' org.qtpass.QtPass.yml > org.qtpass.QtPass.ci.yml
cat >> org.qtpass.QtPass.ci.yml <<'YAML'
    sources:
      - type: dir
        path: ..
        skip:
          - .git
YAML
echo "wrote $(pwd)/org.qtpass.QtPass.ci.yml"
