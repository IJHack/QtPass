#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the libFuzzer targets in tests/fuzz/ against an instrumented copy
# of the static library in src/, with the shared libraries they need at run
# time in OUTDIR/lib, so OUTDIR can be run elsewhere (OSS-Fuzz).
#
#   tests/fuzz/build.sh OUTDIR
#
# Environment (OSS-Fuzz / ClusterFuzzLite set these; defaults are for a
# local ASan + UBSan build):
#   CXX                 clang++
#   CXXFLAGS            sanitizer flags for our code
#   LIB_FUZZING_ENGINE  how to link the fuzzing engine
#   QMAKE               qmake of a Qt 6.8+ (default: qmake6, then qmake)
set -euo pipefail

out="$(mkdir -p "${1:?usage: $0 OUTDIR}" && cd "$1" && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"

: "${CXX:=clang++}"
: "${CXXFLAGS:=-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fsanitize-address-use-after-scope}"
: "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${QMAKE:=$(command -v qmake6 || command -v qmake)}"

# Qt from aqt or a distro is built against libstdc++; OSS-Fuzz's default
# -stdlib=libc++ would not link against it.
CXXFLAGS="${CXXFLAGS//-stdlib=libc++/}"

build="$out/.build"
mkdir -p "$build"
(
	cd "$build"
	# Only the library: the fuzzers link the objects they need from it.
	# A debug build, so Q_ASSERT checks run while fuzzing.
	# -fsanitize=fuzzer-no-link adds coverage instrumentation without main().
	"$QMAKE" "$root/src/src.pro" -spec linux-clang \
		QMAKE_CC="${CC:-${CXX/clang++/clang}}" QMAKE_CXX="$CXX" QMAKE_LINK="$CXX" \
		QMAKE_CXXFLAGS+="$CXXFLAGS -fsanitize=fuzzer-no-link" \
		QMAKE_LFLAGS+="$CXXFLAGS" CONFIG+=debug
	make -j"$(nproc)"
)
lib="$(find "$build" -name libqtpass.a -print -quit)"

qt_headers="$("$QMAKE" -query QT_INSTALL_HEADERS)"
qt_libs="$("$QMAKE" -query QT_INSTALL_LIBS)"

for src in "$here"/fuzz_*.cpp; do
	name="$(basename "$src" .cpp)"
	# --as-needed: a target that only pulls QtCore code from the library
	# needs only QtCore at run time.
	# CXXFLAGS and LIB_FUZZING_ENGINE are lists of flags, split on purpose.
	# shellcheck disable=SC2086
	"$CXX" $CXXFLAGS -std=c++17 -fPIC \
		-I"$root/src" -I"$build" \
		-I"$qt_headers" -I"$qt_headers/QtCore" \
		"$src" "$lib" $LIB_FUZZING_ENGINE \
		-Wl,--as-needed -L"$qt_libs" -lQt6Core \
		-Wl,-rpath,"\$ORIGIN/lib" -Wl,-rpath,"$qt_libs" \
		-o "$out/$name"
done

# The fuzzers must also run where only OUTDIR exists: OSS-Fuzz runs them in
# a separate image without Qt. Copy QtCore and the libraries it pulls in
# that a base system lacks (Qt's ICU, glib, pcre, zstd) into OUTDIR/lib,
# which the $ORIGIN/lib rpath above searches first.
mkdir -p "$out/lib"
for src in "$here"/fuzz_*.cpp; do
	ldd "$out/$(basename "$src" .cpp)" | awk '/=> \//{print $3}'
done | sort -u | while read -r dep; do
	case "$(basename "$dep")" in
	libc.so.* | libm.so.* | libpthread.so.* | libdl.so.* | librt.so.* | \
		libresolv.so.* | libgcc_s.so.* | libstdc++.so.* | ld-linux*) ;;
	*) cp -L "$dep" "$out/lib/" ;;
	esac
done
ls -1 "$out/lib"
