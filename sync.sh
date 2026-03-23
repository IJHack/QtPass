#!/usr/bin/env bash
# QtPass Documentation Sync Script
# Syncs the gh-pages branch content to qtpass.org
#
# Usage:
#   ./sync.sh          # Dry run (preview changes)
#   ./sync.sh --live   # Actually sync to server
#
# Requires: rsync, ssh access to annejan.com

set -euo pipefail

DEST="annejan.com:www/qtpass/"
DRY_RUN=1

if [[ "${1:-}" == "--live" ]]; then
  DRY_RUN=0
fi

ARGS=(
  -avh
  --delete
  --itemize-changes
  --exclude='.git/'
  --exclude='.github/'
  --filter=':- .gitignore'
)

if [[ "$DRY_RUN" -eq 1 ]]; then
  ARGS+=(-n)
  echo "Dry run"
else
  echo "Live deploy"
fi

rsync "${ARGS[@]}" ./ "$DEST"
