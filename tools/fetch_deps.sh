#!/usr/bin/env bash
# Unpack the build dependencies that may be missing from the system (Bullet,
# fmt, OpenAL, glm) into deps/sysroot, without root: the Debian/Ubuntu packages
# are downloaded with `apt-get download` and extracted with `dpkg -x`.
# CMakeLists.txt looks there first; the binary finds the shared libraries
# through its RUNPATH.
#
# Usage: tools/fetch_deps.sh            then: cmake -S . -B build
# Packages already in deps/debs are reused, so it also works offline.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEBS="$ROOT/deps/debs"
SYSROOT="$ROOT/deps/sysroot"

PACKAGES=(
	libbullet-dev libbullet3.24t64
	libfmt-dev libfmt9
	libopenal-dev libopenal1
	libglm-dev
)

mkdir -p "$DEBS" "$SYSROOT"
cd "$DEBS"
for pkg in "${PACKAGES[@]}"; do
	if ! ls "${pkg}"_*.deb >/dev/null 2>&1; then
		echo "downloading $pkg"
		apt-get download "$pkg"
	fi
done

for deb in "$DEBS"/*.deb; do
	echo "unpacking $(basename "$deb")"
	dpkg -x "$deb" "$SYSROOT"
done

echo "done: $SYSROOT"
