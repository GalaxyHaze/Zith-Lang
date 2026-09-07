# Project Memory

Durable, topic-focused notes about the Zith codebase and development workflow.
This complements `docs/` and the code search indexes by recording the non-obvious
contracts, decisions, and gotchas that are not discoverable from the source alone.

## Conventions

- Create one file per topic or area under `memory/`.
- Keep each file within 300 lines, preferably 200-300 lines.
- Split overgrown topics into more focused files and link them from here.
- Read the relevant file before working on that area.
- Update notes when the repository behavior changes.
- Invalidate or rewrite entries that are outdated instead of leaving
  contradictory notes.

## Index

- [comptime-generics-traits.md](comptime-generics-traits.md): archived
  full-Zith 0.7.0 comptime/traits/capability planning; the active `Zith--`
  plan now points to `docs/plans/`.
- [monolith-splits.md](monolith-splits.md): completed frontend/session split
  layout and the remaining large compiler TUs.
- [nra-hir-boundary.md](nra-hir-boundary.md): stable ownership/HIR boundary and
  the pitfalls that keep the pre-HIR NRA contract intact.
- [nra-design.md](nra-design.md): future `Zith` NRA design decisions and link to
  the full `docs/nra-spec.md` contract.
- [platform-imports.md](platform-imports.md): implemented `Zith--` platform
  variants for `foo.<arch>.<os>.zith` plus cache/target decisions.
- [build-c-compile.md](build-c-compile.md): companion `.c` discovery/link flow,
  current backend selection rules, and the Clang `-Weverything` gotchas hit
  while landing the feature.
- [simd-intrinsics-asm.md](simd-intrinsics-asm.md): decision to prefer LLVM
  intrinsics over assembly for native/SIMD support; vector types come after
  scalar intrinsics. Also records the known `@sizeOf` `E5001` lowering gap.
- [stalin-debug.md](stalin-debug.md): strategy for narrowing compiler bugs by
  disabling reproducer/code paths step by step before deep debugging.
- [tests-and-defer-codegen.md](tests-and-defer-codegen.md): test binary
  locations, `defer` codegen notes, and the known modern-file alias codegen
  failure outside the defer work.
- [plan-debt-status.md](plan-debt-status.md): plan/debt/status curation
  contract, the current feature status map, and the docs audit method.
- [flat-containers-cache.md](flat-containers-cache.md): FlatMap/FlatSet API
  contracts, hot-map migrations, cache/CLI `--no-cache`, and validation checks
  landed during the consolidation pass.
- [release-install-layout.md](release-install-layout.md): release artifact
  conventions for `scripts/install.sh`, stdlib discovery paths, and the
  installer fixes landed during the packaging audit.
- [discord-mcp.md](discord-mcp.md): ByteAsk Discord MCP server registration,
  the stdio-only limitation of the v1.0.0 release JAR, and the env-var/token
  requirements to run it.
