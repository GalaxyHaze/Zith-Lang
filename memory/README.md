# Project Memory

Durable, topic-focused notes about the Zith codebase and development workflow.
This complements `docs/` and the code search indexes by recording the non-obvious
contracts, decisions, and gotchas that are not discoverable from the source alone.

## Conventions

- Create one file per topic or area under `memory/`.
- Keep each file as short as useful. Operational notes do not need 200 lines
  unless they carry a real playbook; long design narratives belong in `docs/`.
- Prefer one source of truth per contract: when `docs/` already owns the
  contract, `memory/` records only the non-obvious implementation facts and
  links to `docs/`.
- Read the relevant file before working on that area.
- Update notes when the repository behavior changes.
- Invalidate or rewrite entries that are outdated instead of leaving
  contradictory notes.

## Index

- [audit-cleaning.md](audit-cleaning.md): current repo-hygiene audit state,
  deferred WIP, and the cleaning queue.
- [comptime-generics-traits.md](comptime-generics-traits.md): full-Zith 0.7.0
  comptime/traits/capability context; details live in
  `docs/plans/archive/0.7.0-zith/` and this note now points there.
- [monolith-splits.md](monolith-splits.md): completed frontend/session split
  layout and the remaining large compiler TUs. See
  `docs/plans/monolith-splits.md` for the execution contract.
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
- [branch-protocol-draft.md](branch-protocol-draft.md): full-Zith thread-model
  draft summary (`pThread fork`, `merge`, stdlib `spawn`); avoids treating
  `Branch` as a capability and keeps Zith-- out of core syntax.
- [discord-mcp.md](discord-mcp.md): ByteAsk Discord MCP server registration,
  the stdio-only limitation of the v1.0.0 release JAR, and the env-var/token
  requirements to run it.
- [idea-lifecycle.md](idea-lifecycle.md): file-backed idea lifecycle, ABI
  state map, and the struct-layout unit used for conforming contracts.
- [execution-ir-drawing.md](execution-ir-drawing.md): current drawing status
  and pointer for the execution IR/interpreter contract.
