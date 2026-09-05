# Discord MCP Setup

## Summary

The Zith workspace has a global ByteAsk MCP server (`discord-mcp`) that exposes
65 Discord tools (messages, channels, categories, webhooks, roles, moderation,
voice/stage, scheduled events, permissions, invites, forums, and emojis). It runs
the official `SaseQ/discord-mcp` v1.0.0 JAR in **stdio** mode through the wrapper
at `scripts/discord-mcp-stdio.sh`. The wrapper requires `DISCORD_TOKEN` to be set
in the environment of the process that launches ByteAsk.

## Setup Locations

- MCP registration: `byteask mcp add discord-mcp --env DISCORD_TOKEN=... --env
  DISCORD_MCP_JAR=... -- /home/diogo/Zith/scripts/discord-mcp-stdio.sh`
- JAR: `~/discord-mcp/discord-mcp-1.0.0.jar` (downloaded from the v1.0.0 GitHub
  release).
- Wrapper: `scripts/discord-mcp-stdio.sh`
- Server name reported by the JAR: `discord-mcp-server` (protocol 2024-11-05).

## Runtime Requirements

- Environment variable `DISCORD_TOKEN` must be exported wherever the ByteAsk
  session is spawned. The wrapper fails fast with
  `DISCORD_TOKEN must be set` if it is missing.
- Optional `DISCORD_GUILD_ID` is not required by the wrapper; guild id is
  optional per tool when set on the server.
- The wrapper `cd`s to the JAR directory before launching, so the JAR can create
  `./target/logs/` for its file appender if needed.

## Why stdio, not HTTP

- The v1.0.0 release JAR documents an HTTP profile (`http://localhost:8085/mcp`),
  but the published artifact does not include a servlet engine
  (`spring-web` is present; Tomcat/Jetty/Undertow/Reactor Netty is absent). It
  starts (`Started DiscordMcpApplication ...`) without binding port 8085.
- The JAR's default `spring.ai.mcp.server.stdio=true` and
  `spring.main.web-application-type=none` make the stdio transport the only
  reliable path. ByteAsk launches the wrapper as a stdio MCP server.
- Do not attempt an HTTP/URL registration pointing at `localhost:8085`; there is
  no listener.

## Verified Behavior

- Handshake `initialize` returns
  `protocolVersion: 2024-11-05`,
  `serverInfo: discord-mcp-server`, version `1.0.0`.
- A valid bot token with the Server Members privileged intent enabled passes
  `Login Successful!`, `Connected to WebSocket`, `Finished Loading!`, and the
  `McpServerAutoConfiguration` logs `Registered tools: 65`.
- `tools/list` on the stdio transport returns a `tools` array whose first entry
  is `upsert_member_channel_permissions`. All 65 Discord tools are present.

## Troubleshooting

- `DISCORD_TOKEN must be set` → the wrapper could not read the env var; export it
  in the shell that starts ByteAsk.
- `DISCORD_TOKEN not set` during an earlier HTTP test is expected: subprocesses
  do not inherit env vars exported in a different interactive shell. Export in
  the ByteAsk-launching shell or set it inline in the wrapper invocation.
- Port 8085 not listening with `Started` in the log is the HTTP-profile packaging
  defect described above; it is normal and not a network problem.
- DNS `Temporary failure in name resolution` for `discord.com` appears when the
  JAR runs in a sandbox without network; run it outside the sandbox (it is
  registered as a trusted byteask MCP server).
- `CloseCode 4014 Disallowed Intents` means the bot's privileged
  `GUILD_MEMBERS` intent is not enabled in the Discord Developer Portal. Enable
  the Server Members intent (and optionally Voice States / Scheduled Events)
  and save before retrying.

## Security Notes

- The wrapper stores the token via environment lookup, not in source control. If
  a token is pasted into a chat or commit, rotate it in the Discord Developer
  Portal.
