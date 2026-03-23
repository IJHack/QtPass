#!/usr/bin/env bash
set -euo pipefail

DEST="annejan.com:www/qtpass/"
DRY_RUN=1
DO_DELETE=0

usage() {
  cat <<'EOF'
Usage:
  ./deploy.sh [--live] [--delete]

Options:
  --live     Do the actual deploy
  --delete   Delete remote files that no longer exist locally
  -h, --help Show this help
EOF
}

for arg in "$@"; do
  case "$arg" in
    --live)
      DRY_RUN=0
      ;;
    --delete)
      DO_DELETE=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      usage >&2
      exit 1
      ;;
  esac
done

ARGS=(
  -avh
  --itemize-changes
  --exclude=.git/
  --exclude=.github/
  --filter=':- .gitignore'
)

if [[ "$DRY_RUN" -eq 1 ]]; then
  ARGS+=(-n)
fi

if [[ "$DO_DELETE" -eq 1 ]]; then
  ARGS+=(--delete)
fi

if [[ "$DRY_RUN" -eq 1 ]]; then
  echo "Mode: dry-run"
else
  echo "Mode: live"
fi

if [[ "$DO_DELETE" -eq 1 ]]; then
  echo "Remote deletions: enabled"
else
  echo "Remote deletions: disabled"
fi

rsync "${ARGS[@]}" ./ "$DEST"
