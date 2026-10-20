#!/usr/bin/env bash
# Extract the game's data from the three PC CDs of The Simpsons: Hit & Run and
# put it where the engine reads it (docs/ASSETS.md):
#
#   art/                    the models, levels, menus, missions and sounds
#   art/audio/dialog/       DIALOGF.RCF, in the clear on CD1
#   scripts/cars/           each car's tuning
#   scripts/level0N/        the game's missions; scripts/rewards.mfk
#   movies/                 the cutscenes (.RMV)
#
# Usage: tools/extract_assets.sh <source dir> [target dir]
#
#   <source dir>  holds the three CD images, as .iso or as the .7z archives they
#                 come in, at any depth ("Hit & Run 1.iso", "...cd1-pcwin.7z")
#   [target dir]  where art/ and scripts/ go; the project root by default
#
# The cabinets are InstallShield archives, not Microsoft CAB: unshield reads
# them, cabextract does not. They span the three CDs (DATA1.HDR drives
# DATA1..4.CAB), so they are gathered in one directory first. isoinfo
# (genisoimage) reads the images; unshield and 7z, when missing, are unpacked
# into deps/sysroot without root, as tools/fetch_deps.sh does.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ $# -lt 1 ]; then
	sed -n '2,21p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
	exit 1
fi
SOURCE="$(cd "$1" && pwd)"
TARGET="$(mkdir -p "${2:-$ROOT}" && cd "${2:-$ROOT}" && pwd)"
WORK="$TARGET/files"
SYSROOT="$ROOT/deps/sysroot"

# A tool, from the system or unpacked into deps/sysroot from its packages.
tool() {
	local name="$1"
	shift
	if command -v "$name" >/dev/null 2>&1; then
		command -v "$name"
		return
	fi
	if [ ! -x "$SYSROOT/usr/bin/$name" ]; then
		mkdir -p "$ROOT/deps/debs" "$SYSROOT"
		(cd "$ROOT/deps/debs" && for pkg in "$@"; do
			ls "${pkg}"_*.deb >/dev/null 2>&1 || apt-get download "$pkg" >&2
			dpkg -x "$(ls "${pkg}"_*.deb | head -1)" "$SYSROOT"
		done)
	fi
	echo "$SYSROOT/usr/bin/$name"
}
export LD_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

command -v isoinfo >/dev/null 2>&1 || {
	echo "isoinfo is needed: sudo apt install genisoimage" >&2
	exit 1
}

# The three images: the ISOs, or else the .7z archives, unpacked once.
find_iso() {
	{ find "$SOURCE" "$WORK/cd" -iname "*$1.iso" 2>/dev/null || true; } | head -1
}
for n in 1 2 3; do
	if [ -z "$(find_iso "$n")" ]; then
		archive="$({ find "$SOURCE" -iname "*cd$n*.7z" || true; } | head -1)"
		if [ -z "$archive" ]; then
			echo "no image of CD$n under $SOURCE (an .iso ending in $n, or a .7z with cd$n)" >&2
			exit 1
		fi
		echo "unpacking $(basename "$archive")"
		"$(tool 7z 7zip)" x "$archive" -o"$WORK/cd" -y >/dev/null
	fi
done
ISO1="$(find_iso 1)"
ISO2="$(find_iso 2)"
ISO3="$(find_iso 3)"

# The cabinets, side by side: CD1 holds DATA1.CAB, DATA1.HDR, DATA2.CAB; CD2
# DATA3.CAB; CD3 DATA4.CAB.
mkdir -p "$WORK/cabs"
for f in DATA1.CAB DATA1.HDR DATA2.CAB; do
	isoinfo -i "$ISO1" -x "/$f;1" >"$WORK/cabs/$f"
done
isoinfo -i "$ISO2" -x "/DATA3.CAB;1" >"$WORK/cabs/DATA3.CAB"
isoinfo -i "$ISO3" -x "/DATA4.CAB;1" >"$WORK/cabs/DATA4.CAB"

echo "extracting the cabinets"
UNSHIELD="$(tool unshield unshield libunshield0)"
rm -rf "$WORK/extracted"
# A couple of files do not come out (card38_t.p3d, music02.rcf): unshield says
# so and fails; the game runs without them, and they are mended below.
"$UNSHIELD" -D 3 -d "$WORK/extracted" x "$WORK/cabs/DATA1.CAB" >"$WORK/unshield.log" 2>&1 ||
	echo "unshield could not extract some files; going on" >&2
EX="$WORK/extracted"

# A file stored whole across two cabinets, the end of one and the start of the
# next (music02.rcf: DATA3.CAB then DATA4.CAB), fails to come out: unshield
# looks for all of it in the first. Stored, not compressed, it is the end of
# the one and the start of the other, put back together from where unshield's
# log says each part starts.
"$UNSHIELD" l "$WORK/cabs/DATA1.CAB" >"$WORK/unshield.list" 2>/dev/null || true
python3 -I - "$WORK" <<'PYTHON'
import os, re, sys
work = sys.argv[1]
log = open(os.path.join(work, "unshield.log"), errors="replace").read()
sizes = {}
for line in open(os.path.join(work, "unshield.list"), errors="replace"):
    m = re.match(r"\s*(\d+)\s+(\S.*\S)\s*$", line)
    if m:
        sizes[m.group(2)] = int(m.group(1))
# Each volume's first and last file, and where they start in it.
volumes = {}
for m in re.finditer(r"Open volume (\d+)\n.*?First file index = (\d+), last file index = (\d+)\n"
                     r".*?First file offset = ([0-9a-f]+), last file offset = ([0-9a-f]+)", log):
    volumes[int(m.group(1))] = tuple(int(g, 16) if i > 1 else int(g) for i, g in enumerate(m.groups()[1:]))
for m in set(re.findall(r"of file (\d+) \((.+?)\) from volume (\d+)", log)):
    index, name, volume = int(m[0]), m[1], int(m[2])
    path = next((p for p in sizes if p.replace("\\", "/").endswith("/" + name) or p == name), None)
    first, second = volumes.get(volume), volumes.get(volume + 1)
    if not path or not first or not second or first[1] != index or second[0] != index:
        print(f"{name}: not mended")
        continue
    cabs = os.path.join(work, "cabs")
    one = os.path.join(cabs, f"DATA{volume}.CAB")
    other = os.path.join(cabs, f"DATA{volume + 1}.CAB")
    with open(one, "rb") as f:
        f.seek(first[3])
        data = f.read()
    with open(other, "rb") as f:
        f.seek(second[2])
        data += f.read(max(0, sizes[path] - len(data)))
    # Only an archive whose header shows it was stored as it is.
    if len(data) != sizes[path] or not (name.lower().endswith(".rcf") and data.startswith(b"RADCORE CEMENT LIBRARY")):
        print(f"{name}: not mended")
        continue
    target = os.path.join(work, "extracted", *path.split("\\"))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    open(target, "wb").write(data)
    print(f"{name}: mended from DATA{volume}.CAB and DATA{volume + 1}.CAB")
PYTHON

echo "placing the game's files in $TARGET"
mkdir -p "$TARGET/art/audio/music" "$TARGET/art/audio/dialog"
cp -a "$EX/Art/." "$TARGET/art/"
cp -a "$EX"/Sound/*.rcf "$TARGET/art/audio/"
cp -a "$EX/Sound/sound" "$TARGET/art/audio/"
cp -a "$EX"/Music1/*.rcf "$EX"/Music2/*.rcf "$TARGET/art/audio/music/"
isoinfo -i "$ISO1" -x "/DIALOGF.RCF;1" >"$TARGET/art/audio/dialog/DIALOGF.RCF"

# The movies (.RMV, Bink: UI-FMV), outside the cabinets.
mkdir -p "$TARGET/movies"
for iso in "$ISO1" "$ISO2" "$ISO3"; do
	{ isoinfo -f -i "$iso" | grep -i '^/MOVIES/.*\.RMV;1$' || true; } | while read -r movie; do
		name="$(basename "${movie%;1}")"
		isoinfo -i "$iso" -x "$movie" >"$TARGET/movies/$name"
	done
done

# The cars' tuning and the game's missions, which the repository does not
# carry (scripts/Missions is ours).
mkdir -p "$TARGET/scripts/cars"
cp -a "$EX"/Scripts/cars/*.con "$TARGET/scripts/cars/"
for n in 1 2 3 4 5 6 7; do
	rm -rf "$TARGET/scripts/level0$n"
	cp -a "$EX/Scripts/missions/level0$n" "$TARGET/scripts/level0$n"
done
cp -a "$EX/Scripts/missions/rewards.mfk" "$TARGET/scripts/rewards.mfk"

echo "done: $(find "$TARGET/art" -type f | wc -l) files under $TARGET/art"
