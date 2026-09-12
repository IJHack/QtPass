#!/usr/bin/env bash
# Regenerate sitemap.xml from the top-level pages on the gh-pages branch.
#
# - URLs are the canonical extension-less form (/downloads, not /downloads.html)
# - lastmod comes from the last git commit that touched the file
# - Doxygen output under docs/ is represented by a single /docs/ entry;
#   listing its 600+ generated pages only buried the pages that matter
#
# Usage: ./scripts/update-sitemap.sh   (writes sitemap.xml in place)
set -euo pipefail
cd "$(dirname "$0")/.."

BASE="https://qtpass.org"

priority() {
  case "$1" in
    index) echo 1.0 ;;
    downloads|getting-started) echo 0.9 ;;
    advanced|macos|changelog) echo 0.8 ;;
    privacy) echo 0.5 ;;
    changelog.*|old) echo 0.3 ;;
    *) echo 0.5 ;;
  esac
}

changefreq() {
  case "$1" in
    index|downloads|changelog) echo weekly ;;
    macos) echo monthly ;;
    changelog.*|old|privacy) echo yearly ;;
    *) echo monthly ;;
  esac
}

lastmod() {
  git log -1 --format=%cI -- "$1"
}

entry() {
  local loc="$1" file="$2" name="$3"
  printf '  <url>\n    <loc>%s</loc>\n    <lastmod>%s</lastmod>\n    <changefreq>%s</changefreq>\n    <priority>%s</priority>\n  </url>\n' \
    "$loc" "$(lastmod "$file")" "$(changefreq "$name")" "$(priority "$name")"
}

OUTPUT="sitemap.xml"
{
echo '<?xml version="1.0" encoding="UTF-8"?>'
echo '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'

entry "$BASE/" index.html index

for f in ./*.html; do
  name="${f#./}"; name="${name%.html}"
  case "$name" in
    index|404) continue ;;
  esac
  entry "$BASE/$name" "$f" "$name"
done

entry "$BASE/docs/" docs/index.html docs

echo '</urlset>'
} > "$OUTPUT"

echo "Generated $OUTPUT with $(grep -c '<url>' "$OUTPUT") URLs" >&2
