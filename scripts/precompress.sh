#!/usr/bin/env bash
# Write a gzip twin (name.ext.gz) next to every text asset of the site, for
# the .htaccess rewrite that serves it when the client accepts gzip: the host
# has no mod_deflate. Run before deploying; the .gz files are not in git.
#
# Usage: ./scripts/precompress.sh   (from anywhere; works in the repo root)
set -euo pipefail
cd "$(dirname "$0")/.."

# Start clean so a removed page does not leave a stale twin behind.
git ls-files -o --exclude-standard --ignored -z -- '*.gz' ':!docs/**' | xargs -0 -r rm -f
# Only files the site actually ships: what git tracks, minus the API docs.
git ls-files -z -- '*.html' '*.css' '*.js' '*.svg' '*.xml' '*.json' '*.txt' '*.webmanifest' ':!docs/**' ':!xml/**' ':!package*.json' |
  while IFS= read -r -d '' f; do
    [ "$(stat -c %s "$f")" -gt 1024 ] || continue
    gzip -9 -n -c "$f" > "$f.gz"
    # Not worth the round trip when it barely shrinks.
    if [ "$(stat -c %s "$f.gz")" -ge "$(( $(stat -c %s "$f") * 9 / 10 ))" ]; then
      rm -f "$f.gz"
    fi
  done
echo "precompressed $(git ls-files -o --exclude-standard --ignored -- '*.gz' | grep -vc '^docs/') files" >&2
