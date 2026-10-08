#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT

# Install or remove chatterino.app in a macOS applications folder.
#
#   install-macos.sh <path/to/chatterino.app> [prefix]
#   install-macos.sh --uninstall [prefix]
#
# prefix defaults to /Applications. User settings in
# ~/Library/Application Support/chatterino are never touched.

set -euo pipefail

APP_NAME="chatterino.app"
BUNDLE_ID="com.chatterino"

# Ask a running Chatterino to quit so its files can be replaced cleanly
quit_running() {
    if pgrep -xq chatterino; then
        echo "Quitting running Chatterino..."
        osascript -e "tell application id \"$BUNDLE_ID\" to quit" >/dev/null 2>&1 || true
        for _ in $(seq 1 50); do
            pgrep -xq chatterino || return 0
            sleep 0.1
        done
        echo "Chatterino is still running; close it and try again." >&2
        exit 1
    fi
}

# Use sudo only when the prefix isn't writable by the current user
run_privileged() {
    if [ -w "$PREFIX" ]; then
        "$@"
    else
        sudo "$@"
    fi
}

if [ "${1:-}" = "--uninstall" ]; then
    PREFIX="${2:-/Applications}"
    TARGET="$PREFIX/$APP_NAME"
    if [ ! -d "$TARGET" ]; then
        echo "Nothing to remove at $TARGET"
        exit 0
    fi
    quit_running
    run_privileged rm -rf "$TARGET"
    echo "Removed $TARGET"
    exit 0
fi

SOURCE="${1:?usage: $0 <path/to/chatterino.app> [prefix] | --uninstall [prefix]}"
PREFIX="${2:-/Applications}"
TARGET="$PREFIX/$APP_NAME"

if [ ! -d "$SOURCE" ]; then
    echo "No app bundle at $SOURCE; run 'make' first." >&2
    exit 1
fi

quit_running
mkdir -p "$PREFIX" 2>/dev/null || run_privileged mkdir -p "$PREFIX"

# Copy beside the target first, then swap, so a failed copy never leaves
# the user without a working app
STAGING="$PREFIX/.$APP_NAME.installing"
run_privileged rm -rf "$STAGING"
run_privileged ditto "$SOURCE" "$STAGING"
run_privileged rm -rf "$TARGET"
run_privileged mv "$STAGING" "$TARGET"

# Refresh Launch Services so Spotlight and the Dock pick up the new build
LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
[ -x "$LSREGISTER" ] && "$LSREGISTER" -f "$TARGET" || true

echo "Installed $TARGET"
