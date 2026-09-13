#!/usr/bin/env bash
# Submits a signed .pkg (built by build_installer.sh) for Apple notarization and staples the ticket.
#
# One-time setup (on a real Mac, once per signing identity):
#   xcrun notarytool store-credentials "<profile-name>" \
#       --apple-id "you@example.com" --team-id "TEAMID" --password "<app-specific password>"
#
# Usage:
#   NOTARY_PROFILE="<profile-name>" ./notarize.sh path/to/Crystal Bowie-0.9.0.pkg
#
# Uses `xcrun notarytool` (Apple's current recommended tool). The older `xcrun altool --notarize-app`
# flow was deprecated and shut down by Apple in November 2023 — do not revert to it.

set -euo pipefail

: "${NOTARY_PROFILE:?Set NOTARY_PROFILE to a keychain profile created via 'xcrun notarytool store-credentials' (see the comment above).}"

PKG_PATH="${1:-}"
if [ -z "$PKG_PATH" ]; then
    echo "Usage: $0 path/to/output.pkg" >&2
    exit 1
fi
if [ ! -f "$PKG_PATH" ]; then
    echo "ERROR: $PKG_PATH not found." >&2
    exit 1
fi

echo "Submitting $PKG_PATH for notarization (profile: $NOTARY_PROFILE)..."
xcrun notarytool submit "$PKG_PATH" --keychain-profile "$NOTARY_PROFILE" --wait

echo "Stapling notarization ticket..."
xcrun stapler staple "$PKG_PATH"

echo "Done: $PKG_PATH is signed, notarized, and stapled."
