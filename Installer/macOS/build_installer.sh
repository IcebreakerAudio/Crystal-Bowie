#!/usr/bin/env bash
# Builds a distributable macOS .pkg from already-built Release CLAP/VST3/AU plugin bundles, using
# pkgbuild (one component package per format) + productbuild (combined into one product installer).
#
# Usage: ./build_installer.sh
#
# Requires a macOS Release build to already exist (this script only packages, it doesn't build).
# Requires Xcode command line tools (pkgbuild/productbuild) — Crystal Bowie has never had a macOS build
# performed, so BUILD_DIR/CONFIG_SUBDIR below are a best guess based on JUCE's own CMake output layout
# and need verifying/adjusting on first use (see the comment above them).
#
# Signing: exports DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (TEAMID)" to sign; unset
# to build an unsigned package for local testing. Notarization is a separate step, see notarize.sh.
#
# Reusing this for a future sibling project: nothing here should need editing except BUILD_DIR/
# CONFIG_SUBDIR (both local-only, unverifiable without a Mac build) and resources/*.rtf/*.png. Product
# name/version/publisher/bundle ID/output name all come from the generated ProductInfo.sh (see below) —
# they live in exactly one place, the repo root CMakeLists.txt, not here.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# ============================ Product identity (generated) ============================
PRODUCT_INFO="$REPO_ROOT/build/Installer/ProductInfo.sh"
if [ ! -f "$PRODUCT_INFO" ]; then
    echo "ERROR: $PRODUCT_INFO not found." >&2
    echo "Run 'cmake -B build' at the repo root first — it generates this file from CMakeLists.txt's PRODUCT_NAME/COMPANY_NAME/BUNDLE_ID identity variables." >&2
    exit 1
fi
# shellcheck source=/dev/null
source "$PRODUCT_INFO"
# Provides: PRODUCT_NAME, PRODUCT_VERSION, PUBLISHER, OUTPUT_NAME, BUNDLE_ID_BASE

# ============================ Local-only build-output settings ============================
# UNVERIFIED: Crystal Bowie has never had a macOS build performed. JUCE's own CMake support places
# artefacts under build/CrystalBowie_artefacts/<Config>/<Format>/ on every platform (confirmed on
# Windows); after your first Release build on a Mac, run:
#   find "$REPO_ROOT/build/CrystalBowie_artefacts" -name "${OUTPUT_NAME}.*"
# and adjust BUILD_DIR/CONFIG_SUBDIR below to match what you actually find.
BUILD_DIR="$REPO_ROOT/build/CrystalBowie_artefacts"
CONFIG_SUBDIR="Release"

CLAP_SRC="$BUILD_DIR/$CONFIG_SUBDIR/CLAP/${OUTPUT_NAME}.clap"
VST3_SRC="$BUILD_DIR/$CONFIG_SUBDIR/VST3/${OUTPUT_NAME}.vst3"
AU_SRC="$BUILD_DIR/$CONFIG_SUBDIR/AU/${OUTPUT_NAME}.component"

for src in "$CLAP_SRC" "$VST3_SRC" "$AU_SRC"; do
    if [ ! -e "$src" ]; then
        echo "ERROR: expected build artifact not found: $src" >&2
        echo "Build the Release configuration first, and/or adjust BUILD_DIR/CONFIG_SUBDIR above to match your actual build/ layout." >&2
        exit 1
    fi
done

# ============================ Signing (optional) ============================
SIGNING_IDENTITY="${DEVELOPER_ID_INSTALLER:-}"
SIGN_ARGS=()
if [ -n "$SIGNING_IDENTITY" ]; then
    SIGN_ARGS=(--timestamp --sign "$SIGNING_IDENTITY")
else
    echo "WARNING: DEVELOPER_ID_INSTALLER is not set — building an UNSIGNED package."
    echo "         Fine for local testing; a real distribution build needs a Developer ID Installer certificate."
fi

# ============================ Stage + pkgbuild each format ============================
WORK_DIR="$SCRIPT_DIR/build"
OUTPUT_DIR="$SCRIPT_DIR/Output"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/clap" "$WORK_DIR/vst3" "$WORK_DIR/au" "$OUTPUT_DIR"

cp -R "$CLAP_SRC" "$WORK_DIR/clap/"
cp -R "$VST3_SRC" "$WORK_DIR/vst3/"
cp -R "$AU_SRC" "$WORK_DIR/au/"
chmod -R +r "$WORK_DIR"

pkgbuild --root "$WORK_DIR/clap" \
    --install-location "/Library/Audio/Plug-Ins/CLAP" \
    --identifier "${BUNDLE_ID_BASE}.clap.pkg" \
    --version "$PRODUCT_VERSION" \
    "${SIGN_ARGS[@]}" \
    "$WORK_DIR/package.clap.pkg"

pkgbuild --root "$WORK_DIR/vst3" \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    --identifier "${BUNDLE_ID_BASE}.vst3.pkg" \
    --version "$PRODUCT_VERSION" \
    "${SIGN_ARGS[@]}" \
    "$WORK_DIR/package.vst3.pkg"

pkgbuild --root "$WORK_DIR/au" \
    --install-location "/Library/Audio/Plug-Ins/Components" \
    --identifier "${BUNDLE_ID_BASE}.au.pkg" \
    --version "$PRODUCT_VERSION" \
    "${SIGN_ARGS[@]}" \
    "$WORK_DIR/package.au.pkg"

# ============================ distribution.xml (generated, not hand-maintained) ============================
DISTRIBUTION_XML="$WORK_DIR/distribution.xml"
cat > "$DISTRIBUTION_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <allowed-os-versions>
        <os-version min="10.13.0" />
    </allowed-os-versions>
    <title>${PRODUCT_NAME}</title>
    <welcome file="welcome.rtf" />
    <license file="license.rtf" />
    <background file="background.png" scaling="proportional" />
    <product id="${BUNDLE_ID_BASE}" version="${PRODUCT_VERSION}" />
    <options customize="always" require-scripts="false" rootVolumeOnly="true" />
    <choices-outline>
        <line choice="${BUNDLE_ID_BASE}.clap.pkg" />
        <line choice="${BUNDLE_ID_BASE}.vst3.pkg" />
        <line choice="${BUNDLE_ID_BASE}.au.pkg" />
    </choices-outline>
    <choice id="${BUNDLE_ID_BASE}.clap.pkg" title="CLAP Plugin">
        <pkg-ref id="${BUNDLE_ID_BASE}.clap.pkg">package.clap.pkg</pkg-ref>
    </choice>
    <choice id="${BUNDLE_ID_BASE}.vst3.pkg" title="VST3 Plugin">
        <pkg-ref id="${BUNDLE_ID_BASE}.vst3.pkg">package.vst3.pkg</pkg-ref>
    </choice>
    <choice id="${BUNDLE_ID_BASE}.au.pkg" title="Audio Unit Plugin">
        <pkg-ref id="${BUNDLE_ID_BASE}.au.pkg">package.au.pkg</pkg-ref>
    </choice>
</installer-gui-script>
EOF

# ============================ productbuild: combine into the final product installer ============================
FINAL_PKG="$OUTPUT_DIR/${OUTPUT_NAME}-${PRODUCT_VERSION}.pkg"
productbuild \
    "${SIGN_ARGS[@]}" \
    --distribution "$DISTRIBUTION_XML" \
    --package-path "$WORK_DIR" \
    --resources "$SCRIPT_DIR/resources" \
    "$FINAL_PKG"

echo "Done: $FINAL_PKG"
if [ -z "$SIGNING_IDENTITY" ]; then
    echo "(unsigned — set DEVELOPER_ID_INSTALLER and re-run to sign, then see notarize.sh)"
fi
