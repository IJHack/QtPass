#!/usr/bin/env bash
set -euo pipefail

dryrun=false
while [ $# -gt 0 ]; do
	case "$1" in
	--dryrun)
		dryrun=true
		shift
		;;
	-*)
		echo "Usage: $0 [--dryrun] <release-tag>" >&2
		exit 1
		;;
	*)
		break
		;;
	esac
done

if [ $# -ne 1 ]; then
	echo "Usage: $0 [--dryrun] <release-tag>" >&2
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

files=(*)
for file in "${files[@]}"; do
	[ -f "$file" ] || continue
	case "$file" in
	*.asc) continue ;;
	esac
	[ -e "$file.asc" ] && continue
	if ! gpg --armor --detach-sign -- "$file"; then
		echo "Error: failed to sign file '$file' with gpg" >&2
		exit 1
	fi
	new_asc+=("$file.asc")
done

if [ ${#new_asc[@]} -gt 0 ]; then
	if [ "$dryrun" = true ]; then
		echo "[dryrun] Would upload signature files: ${new_asc[*]}"
	else
		if ! gh release upload "$tag" --repo "$repo" --clobber "${new_asc[@]}"; then
			echo "Error: failed to upload signature files for release '$tag' to repository '$repo': ${new_asc[*]}" >&2
			exit 1
		fi
	fi
fi
