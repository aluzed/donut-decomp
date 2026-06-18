#!/bin/bash
# donut setup script - extracts game assets and builds the project
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== donut Setup ==="

# 1. Check/Create shaders symlink
if [ ! -d "shaders" ]; then
    echo "Creating shaders symlink..."
    ln -s assets/shaders shaders
fi

# 2. Extract assets if ISOs are present
if [ -d "files" ] && ls files/*.7z 2>/dev/null | head -1; then
    echo "Found game CD archives. Extracting..."

    # Build unshield if needed
    if [ ! -f "/tmp/donut_unshield" ]; then
        echo "Building unshield..."
        git clone --depth 1 https://github.com/twogood/unshield.git /tmp/unshield_build 2>/dev/null
        cmake -S /tmp/unshield_build -B /tmp/unshield_build/build -Wno-dev 2>/dev/null
        cmake --build /tmp/unshield_build/build -j$(nproc) 2>/dev/null
        cp /tmp/unshield_build/build/src/unshield /tmp/donut_unshield
    fi

    # Extract ISOs
    echo "Extracting ISOs..."
    mkdir -p /tmp/donut_iso
    for f in files/*.7z; do
        7z x "$f" -o/tmp/donut_iso -y > /dev/null 2>&1
    done

    # Extract CABs from ISOs
    echo "Extracting CABs..."
    mkdir -p /tmp/donut_cabs
    find /tmp/donut_iso -name "*.iso" | while read iso; do
        7z e -so "$iso" data1.cab 2>/dev/null > /tmp/donut_cabs/data1.cab || true
        7z e -so "$iso" data1.hdr 2>/dev/null > /tmp/donut_cabs/data1.hdr || true
        7z e -so "$iso" data2.cab 2>/dev/null > /tmp/donut_cabs/data2.cab || true
        7z e -so "$iso" data3.cab 2>/dev/null > /tmp/donut_cabs/data3.cab || true
        7z e -so "$iso" data4.cab 2>/dev/null > /tmp/donut_cabs/data4.cab || true
    done

    # Extract with unshield
    echo "Extracting game assets..."
    mkdir -p /tmp/donut_assets
    cd /tmp/donut_cabs
    /tmp/donut_unshield -d /tmp/donut_assets x data1.cab 2>/dev/null || true

    # Copy assets to project
    echo "Copying assets..."
    mkdir -p "$SCRIPT_DIR/art/cars" "$SCRIPT_DIR/art/chars" "$SCRIPT_DIR/art/level"
    cp /tmp/donut_assets/Art/*.p3d "$SCRIPT_DIR/art/" 2>/dev/null || true
    cp /tmp/donut_assets/Art/cars/*.p3d "$SCRIPT_DIR/art/cars/" 2>/dev/null || true
    cp /tmp/donut_assets/Art/chars/*.p3d "$SCRIPT_DIR/art/chars/" 2>/dev/null || true
    cp /tmp/donut_assets/Art/chars/*.cho "$SCRIPT_DIR/art/chars/" 2>/dev/null || true

    echo "Assets extracted to art/"
else
    echo "No CD archives found in files/ - skipping asset extraction"
fi

# 3. Build
echo "Building donut..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

echo ""
echo "=== Setup Complete ==="
echo "Run: ./build/bin/donut"
echo "(If using conda: LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu ./build/bin/donut)"
