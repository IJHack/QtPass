#!/usr/bin/env bash
set -euo pipefail

# The site's own host, by name: DNS for qtpass.org (TransIP) says where the
# live site is, so a server move needs no change here.
DEST="qtpass.org:www/qtpass/"
DRY_RUN=1
DO_DELETE=0

usage() {
  cat <<'EOF'
Usage:
  ./sync.sh [--live] [--delete]

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

# gzip twins for the .htaccess rewrite (see scripts/precompress.sh); they are
# gitignored, so include them explicitly before the .gitignore filter.
./scripts/precompress.sh

ARGS=(
  -avh
  --itemize-changes
  # Compare contents: a fresh checkout's mtimes are older than the server's,
  # so the default size+mtime check would skip changed files.
  --checksum
  # .git is a file, not a directory, in a git worktree.
  --exclude=.git
  --exclude=.git/
  --exclude=.github/
  --include='*.gz'
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
