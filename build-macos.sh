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

# Extract version from CMakeLists.txt
VERSION=$(grep 'project(BDG_REC VERSION' CMakeLists.txt | sed 's/.*VERSION \([^ ]*\).*/\1/')
DMG_PATH="dist/BDG_REC_${VERSION}_macOS.dmg"
rm -f "$DMG_PATH"

# Generate LEIA-ME.txt with current version and changelog
PREV_TAG=$(git tag --sort=-v:refname | grep -v "^v${VERSION}$" | head -1)
if [ -n "$PREV_TAG" ]; then
    CHANGES=$(git log "${PREV_TAG}..HEAD" --pretty=format:"- %s" --no-merges | grep -v "^- ci:" | grep -v "^- chore:" | grep -v "^- docs:")
else
    CHANGES=$(git log --pretty=format:"- %s" --no-merges -10 | grep -v "^- ci:" | grep -v "^- chore:" | grep -v "^- docs:")
fi

# Stage files
DMG_STAGE=$(mktemp -d)
cp -R "build-release/BDG_REC_artefacts/Release/BDG rec.app" "$DMG_STAGE/"

# Generate LEIA-ME.txt from template
CHANGES_FILE=$(mktemp)
echo "$CHANGES" > "$CHANGES_FILE"
sed "s/\$VERSION/${VERSION}/g" resources/LEIA-ME.template.txt > "$DMG_STAGE/LEIA-ME.tmp"
sed -e '/\$CHANGES/{r '"$CHANGES_FILE" -e 'd;}' "$DMG_STAGE/LEIA-ME.tmp" > "$DMG_STAGE/LEIA-ME.txt"
rm -f "$DMG_STAGE/LEIA-ME.tmp" "$CHANGES_FILE"
ln -s /Applications "$DMG_STAGE/Applications"
mkdir -p "$DMG_STAGE/.background"
cp "resources/fundo-instalacao.png" "$DMG_STAGE/.background/"

# Create writable DMG
hdiutil create -volname "BDG rec" -srcfolder "$DMG_STAGE" -ov -format UDRW "$DMG_PATH"
rm -rf "$DMG_STAGE"

# Set background, window size, and icon positions
hdiutil attach "$DMG_PATH" -readwrite
VOL_NAME=$(ls /Volumes/ | grep "BDG rec" | head -1)
osascript <<APPLESCRIPT
tell application "Finder"
    tell disk "$VOL_NAME"
        open
        delay 1
        tell container window
            set current view to icon view
            set toolbar visible to false
            set statusbar visible to false
            set bounds to {100, 100, 700, 423}
        end tell
        set opts to icon view options of container window
        set icon size of opts to 80
        set arrangement of opts to not arranged
        set background picture of opts to file ".background:fundo-instalacao.png"
        delay 1
        set position of item "Applications" to {170, 135}
        set position of item "BDG rec.app" to {430, 135}
        set position of item "LEIA-ME.txt" to {500, 135}
        update without registering applications
        close
    end tell
end tell
APPLESCRIPT
hdiutil detach "/Volumes/$VOL_NAME"

# Compress to final DMG
DMG_TMP="${DMG_PATH%.dmg}_rw.dmg"
mv "$DMG_PATH" "$DMG_TMP"
hdiutil convert "$DMG_TMP" -format UDZO -ov -o "$DMG_PATH"
rm -f "$DMG_TMP"

echo
echo "[4/4] Done!"
echo
ls -lh "$DMG_PATH"
echo
echo "============================================"
echo " DMG: $DMG_PATH"
echo "============================================"
