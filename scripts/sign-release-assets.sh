#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
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
	if ! gpg_output=$(gpg --armor --detach-sign -- "$file" 2>&1 >/dev/null); then
		echo "Error: failed to sign file '$file' with gpg: $gpg_output" >&2
		exit 1
	fi
	new_asc+=("$file.asc")
done

if [ ${#new_asc[@]} -gt 0 ]; then
	if [ "$dryrun" = true ]; then
		echo "[dryrun] Would upload signature files: ${new_asc[*]}"
	else
		gh_output="$(
			set +e
			output="$(
				gh release upload "$tag" --repo "$repo" --clobber "${new_asc[@]}" 2>&1
			)"
			status=$?
			printf '%s\n' "$status"
			printf '%s\n' "$output"
		)"
		gh_status=$(printf '%s\n' "$gh_output" | sed -n '1p')
		gh_error_output=$(printf '%s\n' "$gh_output" | sed '1d')
		if [ "$gh_status" -ne 0 ]; then
			echo "Error: failed to upload signature files for release '$tag' to repository '$repo': ${new_asc[*]}" >&2
			if [ -n "$gh_error_output" ]; then
				echo "gh error output:" >&2
				echo "$gh_error_output" >&2
			fi
			exit 1
		fi
	fi
fi
