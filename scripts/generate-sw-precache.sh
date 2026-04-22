#!/bin/bash
# Generate service worker precache list from HTML files

OUTPUT="sw-auto.js"

cat > "$OUTPUT" << 'EOF'
const PRECACHE_URLS = [
EOF

for file in $(find . -name "*.html" -type f | grep -v "^./docs/" | grep -v "^./build/" | sort); do
    path="${file#./}"
    # Skip if already in main sw.js (like index.html handled separately)
    if [ "$path" = "index.html" ] || [ "$path" = "404.html" ]; then
        continue
    fi
    echo "  \"/$path\"," >> "$OUTPUT"
done

# Add static assets
cat >> "$OUTPUT" << 'ENDL'
  "/stylesheets/pygment_trac.css",
  "/stylesheets/styles.css",
  "/javascripts/main.js",
  "/javascripts/scale.fix.js",
ENDL

# Add images
find ./images -type f 2>/dev/null | sort | while read -r file; do
    path="${file#./}"
    echo "  \"/$path\"," >> "$OUTPUT"
done

# Add fonts
find ./fonts -type f 2>/dev/null | sort | while read -r file; do
    path="${file#./}"
    echo "  \"/$path\"," >> "$OUTPUT"
done

# Add manifest
[ -f manifest.json ] && echo '  "/manifest.json",' >> "$OUTPUT"
[ -f favicon.ico ] && echo '  "/favicon.ico",' >> "$OUTPUT"

echo '];' >> "$OUTPUT"

echo "Generated $OUTPUT with $(grep -c '"/' "$OUTPUT") URLs"
echo "Copy the PRECACHE_URLS array into sw.js"