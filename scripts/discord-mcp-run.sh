#!/usr/bin/env bash
set -euo pipefail

# discord-mcp v1.0.0 runner (HTTP profile)
# 1) Set DISCORD_TOKEN below or export it in your shell.
# 2) Run this script; server stays on http://localhost:8085/mcp

: "${DISCORD_TOKEN:?Set DISCORD_TOKEN (e.g. export DISCORD_TOKEN=...)}"
DISCORD_GUILD_ID="${DISCORD_GUILD_ID:-}"
JAR="${DISCORD_MCP_JAR:-$HOME/discord-mcp/discord-mcp-1.0.0.jar}"

if [[ ! -f "$JAR" ]]; then
    echo "JAR not found: $JAR" >&2
    echo "Set DISCORD_MCP_JAR to the jar path." >&2
    exit 1
fi

echo "Starting discord-mcp at http://localhost:8085/mcp"
SPRING_PROFILES_ACTIVE=http \
DISCORD_TOKEN="$DISCORD_TOKEN" \
DISCORD_GUILD_ID="$DISCORD_GUILD_ID" \
java -jar "$JAR"
