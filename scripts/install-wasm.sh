#!/bin/bash
set -e

REPO="GalaxyHaze/Zith"
VERSION=""
INSTALL_DIR="${ZITH_WASM_DIR:-$HOME/.zithc-wasm}"

usage() {
    echo "Usage: $0 [<version>] [--dir <path>]"
    echo "  <version>     Specific version to install (default: latest)"
    echo "  --dir <path>  Installation directory (default: \$HOME/.zithc-wasm)"
    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        --dir) INSTALL_DIR="$2"; shift 2 ;;
        --help|-h) usage ;;
        *) VERSION="$1"; shift ;;
    esac
done

if [ -n "$VERSION" ]; then
    echo "Installing requested version: $VERSION"
else
    echo "No version specified. Fetching latest version..."

    if [ -n "$GITHUB_TOKEN" ]; then
        VERSION=$(curl -sH "Authorization: token $GITHUB_TOKEN" \
            "https://api.github.com/repos/$REPO/releases/latest" \
            | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
    else
        VERSION=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" \
            | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
    fi

    if [ -z "$VERSION" ]; then
        echo "Error: Could not fetch latest version from GitHub (API rate limit?)." >&2
        echo "Please specify a version manually: $0 v1.0.0" >&2
        exit 1
    fi
    echo "Latest version found: $VERSION"
fi

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/zithc-wasm.zip"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

echo "Downloading $DOWNLOAD_URL..."
curl -fsSL "$DOWNLOAD_URL" -o "$TMP_DIR/zithc-wasm.zip"

echo "Extracting to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"
unzip -o "$TMP_DIR/zithc-wasm.zip" -d "$INSTALL_DIR"

echo ""
echo "Zith WebAssembly v${VERSION#v} installed to $INSTALL_DIR"
echo ""
echo "Files:"
ls -1 "$INSTALL_DIR"
echo ""
echo "To serve locally:"
echo "  cd $INSTALL_DIR && python3 -m http.server 8080"
echo "Then open http://localhost:8080 in your browser."
echo ""
echo "To embed in a project, copy the files from $INSTALL_DIR to your project."
