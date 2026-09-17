#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build a relocatable QtPass AppImage.
#
# Unlike the Flatpak, an AppImage is not sandboxed: it runs as a normal
# process on the host, so QtPass keeps using the host's pass, gpg2, git and
# gpg-agent (Util::findBinaryInPath() resolves them from $PATH). Only Qt and
# the C++ runtime are bundled. Nothing is bundled that the user is expected
# to already have as a working password store.
#
# Usage:
#   ./scripts/build-appimage.sh [output-directory]
#
# Environment:
#   VERSION   override the version string (default: VERSION from qtpass.pri)
#   ARCH      architecture (default: uname -m). It has to be the host's:
#             linuxdeploy runs here and inspects the freshly built binary,
#             so there is no cross-building an AppImage this way.
#   JOBS      parallel make jobs (default: nproc)

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out_dir="$(mkdir -p "${1:-${repo_root}/dist}" && cd "${1:-${repo_root}/dist}" && pwd)"

build_dir="${repo_root}/build-appimage"
appdir="${build_dir}/AppDir"
tools_dir="${build_dir}/tools"

: "${ARCH:=$(uname -m)}"
: "${JOBS:=$(nproc 2>/dev/null || echo 2)}"

if [ -z "${VERSION:-}" ]; then
	VERSION="$(awk -F'=' '/^VERSION[[:space:]]*=/{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
		"${repo_root}/qtpass.pri")"
fi
if [ -z "${VERSION}" ]; then
	echo "Error: no VERSION found in qtpass.pri and none given." >&2
	exit 1
fi

qmake_bin=""
for candidate in "${QMAKE:-}" qmake6 qmake; do
	[ -n "${candidate}" ] || continue
	if command -v "${candidate}" >/dev/null 2>&1; then
		# Qt 5's qmake is still called "qmake" on many distributions.
		if "${candidate}" -query QT_VERSION 2>/dev/null | grep -q '^6\.'; then
			qmake_bin="$(command -v "${candidate}")"
			break
		fi
	fi
done
if [ -z "${qmake_bin}" ]; then
	echo "Error: no Qt 6 qmake found. Install Qt 6.8 or newer, or set QMAKE." >&2
	exit 1
fi

echo "==> QtPass ${VERSION} (${ARCH}) using ${qmake_bin}"

# Start from a clean build, but keep the downloaded tools between runs.
if [ -d "${build_dir}" ]; then
	find "${build_dir}" -mindepth 1 -maxdepth 1 ! -name tools -exec rm -rf {} +
fi
mkdir -p "${appdir}" "${tools_dir}"

# 1. Build and stage. main/main.pro already installs the binary, desktop file,
#    metainfo, icons and manpage under $PREFIX, so a staged install with
#    PREFIX=/usr produces exactly the AppDir layout linuxdeploy expects.
echo "==> Building"
(
	cd "${build_dir}"
	"${qmake_bin}" "${repo_root}/qtpass.pro" CONFIG+=release PREFIX=/usr
	# sub-main pulls in sub-src through main.depends; the test suite is not
	# packaged, so building it here would be wasted time.
	make -j"${JOBS}" sub-main
	# Only main/ has install targets (binary, desktop file, metainfo, icons,
	# manpage); src/ is a static library and tests/ installs nothing.
	make -C main install INSTALL_ROOT="${appdir}"
)

binary="${appdir}/usr/bin/qtpass"
desktop="${appdir}/usr/share/applications/qtpass.desktop"
icon="${appdir}/usr/share/icons/hicolor/scalable/apps/qtpass-icon.svg"
for required in "${binary}" "${desktop}" "${icon}"; do
	if [ ! -f "${required}" ]; then
		echo "Error: expected ${required} after install, but it is missing." >&2
		exit 1
	fi
done

# 2. Validate the AppStream metadata ourselves. appimagetool would do it too,
#    but its check insists on usr/share/metainfo/<id>.appdata.xml, and the
#    file is installed as qtpass.appdata.xml because the RPM spec and the
#    Flatpak manifest (rename-appdata-file) expect that name. Renaming it
#    everywhere is a separate change, so appimagetool's own check is turned
#    off below and the file is validated here when appstreamcli is around.
if command -v appstreamcli >/dev/null 2>&1; then
	echo "==> Validating AppStream metadata"
	appstreamcli validate --no-net "${appdir}/usr/share/metainfo/qtpass.appdata.xml"
fi

# 3. Fetch linuxdeploy, its Qt plugin and the AppImage runtime: pinned
#    releases, checked against the SHA-256 GitHub publishes for the asset.
#    Bump the three tags and the sums together.
linuxdeploy_tag="1-alpha-20251107-1"
plugin_qt_tag="1-alpha-20250213-1"
runtime_tag="20251108"
case "${ARCH}" in
x86_64)
	linuxdeploy_sha="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
	plugin_qt_sha="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
	runtime_sha="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"
	;;
aarch64)
	linuxdeploy_sha="620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff"
	plugin_qt_sha="bf1c24aff6d749b5cf423afad6f15abd4440f81dec1aab95706b25f6667cdcf1"
	runtime_sha="00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444"
	;;
*)
	echo "Error: no pinned linuxdeploy checksums for ${ARCH}." >&2
	exit 1
	;;
esac

# fetch_tool NAME URL SHA256: download into the tools cache unless the copy
# there already has the expected checksum; refuse anything that does not.
fetch_tool() {
	local name="$1" url="$2" sha="$3" dest="${tools_dir}/$1"
	if [ -f "${dest}" ] && echo "${sha}  ${dest}" | sha256sum --check --quiet --status; then
		printf '%s' "${dest}"
		return
	fi
	# stdout is captured by the caller, so progress goes to stderr.
	echo "==> Fetching ${name}" >&2
	curl --fail --location --silent --show-error --output "${dest}.part" "${url}"
	if ! echo "${sha}  ${dest}.part" | sha256sum --check --quiet --status; then
		echo "Error: ${name} does not match its pinned SHA-256." >&2
		rm -f "${dest}.part"
		exit 1
	fi
	chmod +x "${dest}.part"
	mv "${dest}.part" "${dest}"
	printf '%s' "${dest}"
}

linuxdeploy="$(fetch_tool "linuxdeploy-${ARCH}.AppImage" \
	"https://github.com/linuxdeploy/linuxdeploy/releases/download/${linuxdeploy_tag}/linuxdeploy-${ARCH}.AppImage" \
	"${linuxdeploy_sha}")"
fetch_tool "linuxdeploy-plugin-qt-${ARCH}.AppImage" \
	"https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${plugin_qt_tag}/linuxdeploy-plugin-qt-${ARCH}.AppImage" \
	"${plugin_qt_sha}" >/dev/null
# appimagetool downloads this itself when not given one, in the middle of
# packaging and without a timeout; fetching it here keeps that step offline.
runtime="$(fetch_tool "runtime-${ARCH}" \
	"https://github.com/AppImage/type2-runtime/releases/download/${runtime_tag}/runtime-${ARCH}" \
	"${runtime_sha}")"

# The plugin is discovered on PATH by linuxdeploy, not passed as a path.
export PATH="${tools_dir}:${PATH}"

# 4. Bundle Qt and produce the AppImage.
#    Extracting instead of mounting keeps this working in containers and CI
#    runners without FUSE; the AppImage we emit is unaffected.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${qmake_bin}"
export OUTPUT="QtPass-${VERSION}-${ARCH}.AppImage"
# QtPass draws its toolbar/tray icons from Qt's SVG image plugin; without this
# the theme falls back to blank icons on hosts that lack qt6-svg.
export EXTRA_QT_PLUGINS="svg"
export LDAI_RUNTIME_FILE="${runtime}"
# See step 2: the metadata is validated above, under the name it ships with.
export LDAI_NO_APPSTREAM=1

echo "==> Bundling Qt and packaging"
(
	cd "${out_dir}"
	"${linuxdeploy}" \
		--appdir "${appdir}" \
		--executable "${binary}" \
		--desktop-file "${desktop}" \
		--icon-file "${icon}" \
		--plugin qt \
		--output appimage
)

echo "==> Wrote ${out_dir}/${OUTPUT}"
