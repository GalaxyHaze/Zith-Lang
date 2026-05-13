#!/bin/bash
set -e

REPO="GalaxyHaze/Zith"
VERSION=""
USE_MUSL=false
OUTPUT_NAME="zith"

usage() {
    echo "Usage: $0 [--musl] [<version>]"
    echo "  --musl        Download the musl-linked static binary"
    echo "  <version>     Specific version to install (default: latest)"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --musl) USE_MUSL=true ;;
        --help|-h) usage ;;
        *) VERSION="$arg" ;;
    esac
done

detect_latest_version() {
    # Try authenticated request first (spares rate limit), fall back to unauthenticated
    API_URL="https://api.github.com/repos/$REPO/releases/latest"
    if [ -n "$GITHUB_TOKEN" ]; then
        VERSION=$(curl -sH "Authorization: token $GITHUB_TOKEN" "$API_URL" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
    else
        VERSION=$(curl -s "$API_URL" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
    fi

    if [ -z "$VERSION" ]; then
        echo "Error: Could not fetch latest version from GitHub (API rate limit?)." >&2
        echo "Please specify a version manually: $0 v1.0.0" >&2
        exit 1
    fi
}

if [ -n "$VERSION" ]; then
    echo "Installing requested version: $VERSION"
else
    echo "No version specified. Fetching latest version..."
    detect_latest_version
    echo "Latest version found: $VERSION"
fi

OS="$(uname -s)"
ARCH="$(uname -m)"
FILE_NAME=""

case "$OS" in
    Linux*)
        if [ "$USE_MUSL" = true ]; then
            case "$ARCH" in
                x86_64)   FILE_NAME="zith-linux-amd64-musl" ;;
                aarch64|arm64) FILE_NAME="zith-linux-arm64-musl" ;;
                *) echo "Architecture not supported on Linux (musl): $ARCH" >&2; exit 1 ;;
            esac
        else
            case "$ARCH" in
                x86_64)   FILE_NAME="zith-linux-amd64" ;;
                aarch64|arm64) FILE_NAME="zith-linux-arm64" ;;
                *) echo "Architecture not supported on Linux: $ARCH" >&2; exit 1 ;;
            esac
        fi
        ;;
    Darwin*)
        case "$ARCH" in
            x86_64) FILE_NAME="zith-macos-amd64" ;;
            arm64)  FILE_NAME="zith-macos-arm64" ;;
            *) echo "Architecture not supported on macOS: $ARCH" >&2; exit 1 ;;
        esac
        ;;
    MINGW*|MSYS*|CYGWIN*)
        FILE_NAME="zith-windows-amd64.exe"
        OUTPUT_NAME="zith.exe"
        ;;
    *) echo "OS not supported: $OS" >&2; exit 1 ;;
esac

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$FILE_NAME"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
TMP_FILE="$TMP_DIR/$OUTPUT_NAME"

echo "Downloading $FILE_NAME..."
if ! curl -fsSL "$DOWNLOAD_URL" -o "$TMP_FILE"; then
    echo "Error: Failed to download binary." >&2
    echo "Please check the URL: $DOWNLOAD_URL" >&2
    exit 1
fi

chmod +x "$TMP_FILE"

case "$OS" in
    MINGW*|MSYS*|CYGWIN*)
        cp "$TMP_FILE" "./$OUTPUT_NAME"
        echo "Download complete: ./$OUTPUT_NAME"
        echo "Please move '$OUTPUT_NAME' to a folder in your PATH."
        ;;
    *)
        echo "Installing Zith to /usr/local/bin/..."
        if sudo mv "$TMP_FILE" /usr/local/bin/zith; then
            echo "Installation complete! Run 'zith --help' to get started."
        else
            echo "Installation failed. Check sudo permissions or try manually moving the file." >&2
            exit 1
        fi
        ;;
esac
