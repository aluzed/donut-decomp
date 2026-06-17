#!/bin/bash
# Extract Simpsons Hit & Run assets from original PC CD ISOs
# Usage: ./tools/extract_assets.sh <path_to_cd1_7z> <path_to_cd2_7z> <path_to_cd3_7z> <output_dir>

set -e

if [ $# -lt 4 ]; then
    echo "Usage: $0 <cd1.7z> <cd2.7z> <cd3.7z> <output_dir>"
    exit 1
fi

CD1="$1"
CD2="$2"
CD3="$3"
OUT="$4"
TMP="/tmp/donut_extract_$$"

mkdir -p "$OUT/art/level" "$OUT/art/chars" "$OUT/art/frontend" "$OUT/art/cars"
mkdir -p "$OUT/audio/dialog" "$OUT/audio/music" "$OUT/audio/ambience"
mkdir -p "$OUT/audio/soundfx" "$OUT/audio/nis" "$OUT/audio/scripts" "$OUT/audio/carsound"
mkdir -p "$OUT/movies"
mkdir -p "$TMP"

extract_iso() {
    local archive="$1"
    local dest="$2"
    echo "Extracting $archive..."
    mkdir -p "$dest"
    7z x "$archive" -o"$dest" -y > /dev/null
}

extract_cabs() {
    local iso_dir="$1"
    echo "Extracting CAB files from $iso_dir..."
    for cab in "$iso_dir"/*.CAB; do
        [ -f "$cab" ] && cabextract "$cab" -d "$OUT" 2>/dev/null || true
    done
    for cab in "$iso_dir"/DATA*.CAB; do
        [ -f "$cab" ] && cabextract "$cab" -d "$OUT" 2>/dev/null || true
    done
}

copy_rcf() {
    local iso_dir="$1"
    echo "Copying RCF files from $iso_dir..."
    for rcf in "$iso_dir"/*.RCF; do
        [ -f "$rcf" ] && cp "$rcf" "$OUT/audio/dialog/"
    done
    for rcf in "$iso_dir"/DIALOG*.RCF; do
        [ -f "$rcf" ] && cp "$rcf" "$OUT/audio/dialog/"
    done
}

copy_movies() {
    local iso_dir="$1"
    if [ -d "$iso_dir/MOVIES" ]; then
        echo "Copying movies from $iso_dir..."
        cp "$iso_dir"/MOVIES/*.RMV "$OUT/movies/" 2>/dev/null || true
    fi
}

# Process each CD
for i in 1 2 3; do
    var="CD$i"
    archive="${!var}"
    [ -z "$archive" ] && continue
    extract_iso "$archive" "$TMP/cd$i"
    extract_cabs "$TMP/cd$i"
    copy_rcf "$TMP/cd$i"
    copy_movies "$TMP/cd$i"
done

rm -rf "$TMP"

echo "Done! Assets extracted to $OUT"
echo "Run: cd donut && ln -s $OUT assets"
echo "Or copy: cp -r $OUT/* assets/"
