#!/usr/bin/env bash
set -euo pipefail

# Launch discord-mcp v1.0.0 in stdio mode for ByteAsk.
# Reads DISCORD_TOKEN from the launching environment (must be exported).

: "${DISCORD_TOKEN:?DISCORD_TOKEN must be set}"
JAR="${DISCORD_MCP_JAR:-$HOME/discord-mcp/discord-mcp-1.0.0.jar}"

if [[ ! -f "$JAR" ]]; then
    echo "JAR not found: $JAR (set DISCORD_MCP_JAR)" >&2
    exit 1
fi

cd "$(dirname "$JAR")"
exec env SPRING_PROFILES_ACTIVE=stdio \
    DISCORD_TOKEN="$DISCORD_TOKEN" \
    java -jar "$JAR"
