#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
	echo "Usage: $0 <release-tag>" >&2
	exit 1
fi

tag="$1"
repo="IJHack/QtPass"

workdir="$(mktemp -d -t "sign-release-${tag}.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT

cd "$workdir"

gh release download "$tag" --repo "$repo" --pattern '*'
gh release download "$tag" --repo "$repo" --archive tar.gz
gh release download "$tag" --repo "$repo" --archive zip

new_asc=()

for file in *; do
	[ -f "$file" ] || continue
	case "$file" in
	*.asc) continue ;;
	esac
	[ -e "$file.asc" ] && continue
	gpg --armor --detach-sign -- "$file"
	new_asc+=("$file.asc")
done

if [ ${#new_asc[@]} -gt 0 ]; then
	gh release upload "$tag" --repo "$repo" --clobber "${new_asc[@]}"
fi
