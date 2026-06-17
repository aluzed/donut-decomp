#!/bin/bash
# Extract game assets from Simpsons Hit & Run InstallShield CABs (CD1)
# Requires: unshield (built from source or installed)

set -e

UNSHIELD="/tmp/opencode/unshield/build/src/unshield"
OUTDIR="./assets"

if [ ! -f "$UNSHIELD" ]; then
    echo "Building unshield..."
    git clone --depth 1 https://github.com/twogood/unshield.git /tmp/unshield_build
    cmake -S /tmp/unshield_build -B /tmp/unshield_build/build
    cmake --build /tmp/unshield_build/build -j$(nproc)
    UNSHIELD="/tmp/unshield_build/build/src/unshield"
fi

echo "Extracting from CD1 ISO..."
mkdir -p /tmp/cab_extract

# Extract CAB files from ISO
7z e -so "Hit & Run 1.iso" data1.cab > /tmp/cab_extract/data1.cab 2>/dev/null
7z e -so "Hit & Run 1.iso" data1.hdr > /tmp/cab_extract/data1.hdr 2>/dev/null
7z e -so "Hit & Run 1.iso" data2.cab > /tmp/cab_extract/data2.cab 2>/dev/null

# Copy DIALOGF.RCF
7z e -so "Hit & Run 1.iso" dialogf.rcf > "$OUTDIR/audio/"dialogf.rcf 2>/dev/null

echo "Extracting CAB contents..."
$UNSHIELD -d "$OUTDIR" x /tmp/cab_extract/data1.cab

echo "Done! Check $OUTDIR"
echo ""
echo "Known game files inside the CAB:"
echo "  Art/*.p3d        - game models, textures, levels"
echo "  Executables/Simpsons.exe - main game binary"
echo "  binkw32.dll, pddidx8r.dll, eax.dll - game DLLs"
