#!/bin/bash
set -e

echo "============================================"
echo " BDG rec - macOS Build Script"
echo "============================================"
echo

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found. Install with: brew install cmake"
    exit 1
fi

echo "[1/4] Configuring (universal binary arm64+x86_64)..."
cmake -B build-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

echo
echo "[2/4] Building Release..."
cmake --build build-release --target BDG_REC --config Release

echo
echo "[3/4] Creating DMG..."
mkdir -p dist

DMG_DIR=$(mktemp -d)
cp -R "build-release/BDG_REC_artefacts/Release/BDG rec.app" "$DMG_DIR/"
ln -s /Applications "$DMG_DIR/Applications"

hdiutil create \
    -volname "BDG rec" \
    -srcfolder "$DMG_DIR" \
    -ov \
    -format UDZO \
    "dist/BDG_REC_1.0.0_macOS.dmg"

rm -rf "$DMG_DIR"

echo
echo "[4/4] Done!"
echo
ls -lh dist/BDG_REC_1.0.0_macOS.dmg
echo
echo "============================================"
echo " DMG: dist/BDG_REC_1.0.0_macOS.dmg"
echo "============================================"
