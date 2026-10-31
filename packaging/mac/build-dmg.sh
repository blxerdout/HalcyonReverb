#!/bin/bash
# Packages the release build into Reblxerd-<version>-mac.dmg containing an installer
# (.pkg) that puts the VST3 and AU in /Library/Audio/Plug-Ins and the app in /Applications.
#
#   packaging/mac/build-dmg.sh <build dir> <version label> <output dir>
#
# Signing is optional. Set these to sign and notarize:
#   DEVELOPER_ID_APP        "Developer ID Application: Name (TEAMID)"
#   DEVELOPER_ID_INSTALLER  "Developer ID Installer: Name (TEAMID)"
#   NOTARY_PROFILE          a keychain profile made with `xcrun notarytool store-credentials`
set -euo pipefail

BUILD_DIR="$1"
VERSION="$2"
OUT_DIR="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
ARTEFACTS="$BUILD_DIR/HalcyonReverb_artefacts/Release"
NAME="Reblxerd-$VERSION-mac"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Keep macOS file metadata out of the archives (it shows up as "._" files).
export COPYFILE_DISABLE=1

# Lay the payload out as it will sit on disk.
ROOT="$WORK/root"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Applications"
ditto --noextattr --noqtn "$ARTEFACTS/VST3/Reblxerd.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/Reblxerd.vst3"
ditto --noextattr --noqtn "$ARTEFACTS/AU/Reblxerd.component" "$ROOT/Library/Audio/Plug-Ins/Components/Reblxerd.component"
ditto --noextattr --noqtn "$ARTEFACTS/Standalone/Reblxerd.app" "$ROOT/Applications/Reblxerd.app"

xattr -cr "$ROOT"

# Seal every bundle: with a Developer ID if there is one, otherwise ad-hoc
# (Apple Silicon won't load unsigned code at all).
for bundle in "$ROOT"/Library/Audio/Plug-Ins/*/Reblxerd.* "$ROOT/Applications/Reblxerd.app"; do
    if [[ -n "${DEVELOPER_ID_APP:-}" ]]; then
        codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APP" "$bundle"
    else
        codesign --force --deep --sign - "$bundle"
    fi
done

# Without this, Installer "relocates" a bundle to wherever it finds another copy with
# the same bundle ID (such as a build folder) instead of installing it where it belongs.
pkgbuild --analyze --root "$ROOT" "$WORK/components.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/components.plist" > /dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$WORK/components.plist" 2> /dev/null || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/components.plist"
    i=$((i + 1))
done

PKG_SIGN=()
[[ -n "${DEVELOPER_ID_INSTALLER:-}" ]] && PKG_SIGN=(--sign "$DEVELOPER_ID_INSTALLER")

pkgbuild --root "$ROOT" --component-plist "$WORK/components.plist" \
         --scripts "$HERE/scripts" \
         --identifier com.creativespxce.reblxerd --version "$VERSION" \
         --install-location / ${PKG_SIGN[@]+"${PKG_SIGN[@]}"} "$WORK/Reblxerd.pkg"

DMG_ROOT="$WORK/dmg"
mkdir -p "$DMG_ROOT"
cp "$WORK/Reblxerd.pkg" "$DMG_ROOT/Install Reblxerd.pkg"
cp "$HERE/Read Me.txt" "$DMG_ROOT/Read Me.txt"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/$NAME.dmg"
hdiutil create -volname "Reblxerd $VERSION" -srcfolder "$DMG_ROOT" -fs HFS+ -format UDZO "$OUT_DIR/$NAME.dmg" > /dev/null

if [[ -n "${DEVELOPER_ID_APP:-}" ]]; then
    codesign --sign "$DEVELOPER_ID_APP" --timestamp "$OUT_DIR/$NAME.dmg"
fi
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    xcrun notarytool submit "$OUT_DIR/$NAME.dmg" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$OUT_DIR/$NAME.dmg"
fi

echo "$OUT_DIR/$NAME.dmg"
