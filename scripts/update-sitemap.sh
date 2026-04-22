#!/bin/bash
# Generate sitemap.xml from HTML files

SITE="https://qtpass.org"
OUTPUT="sitemap.xml"

echo '<?xml version="1.0" encoding="UTF-8"?>
<urlset
      xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.sitemaps.org/schemas/sitemap/0.9
            http://www.sitemaps.org/schemas/sitemap/0.9/sitemap.xsd">' > "$OUTPUT"

for file in $(find . -name "*.html" -type f | grep -v "^./docs/" | sort); do
    path="${file#./}"
    loc="$SITE/$path"
    # Use file mtime for lastmod
    mtime=$(date -r "$file" +"%Y-%m-%dT%H:%M:%S+00:00")
    priority="0.50"
    case "$path" in
        index.html) priority="1.00" ;;
        downloads.html|downloads) priority="0.90" ;;
        changelog*.html) priority="0.80" ;;
        getting-started.html|advanced.html|docs) priority="0.80" ;;
        404.html) priority="0.10" ;;
    esac
    echo "<url>
  <loc>$loc</loc>
  <lastmod>$mtime</lastmod>
  <priority>$priority</priority>
</url>" >> "$OUTPUT"
done

echo "</urlset>" >> "$OUTPUT"

echo "Generated $OUTPUT with $(grep -c '<url>' "$OUTPUT") URLs"