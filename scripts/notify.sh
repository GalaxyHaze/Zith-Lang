#!/bin/bash
# notify.sh - Show desktop notifications
# Usage: ./notify.sh "Title" "Message"

set -e

TITLE="${1:-Zith}"
MESSAGE="${2:-Build complete}"
ICON="${3:-dialog-information}"

if command -v notify-send &> /dev/null; then
    notify-send -i "$ICON" "$TITLE" "$MESSAGE"
elif command -v zenity &> /dev/null; then
    zenity --notification --text="$MESSAGE"
elif command -v dunstify &> /dev/null; then
    dunstify "$TITLE" "$MESSAGE"
else
    echo "No notification tool found. Install: notify-send (libnotify), zenity, or dunstify"
    exit 1
fi