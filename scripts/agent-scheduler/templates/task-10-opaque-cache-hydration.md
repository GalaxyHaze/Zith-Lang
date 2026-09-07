# {AGENT}: {TASK}

## Goal

Allow bare `opaque` values imported from other modules or rehydrated from the
cache to retain a stable canonical tag and a valid module-local runtime id.

## Context

- Plan: implementation debt, opaque cache/cross-module hydration.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "Bare `opaque` não pode ser
  re-hidratado no cache/cross-module".
- Current state: module-local `opaque` works, canonical type ids exist, and the
  cache already persists `canonical_mappings`; imported or cached `opaque`
  values can still be rejected with `E2010`.
- Blocking points include `src/session/persistent-cache.cpp`,
  `src/session/compilation-session.cpp`, `src/types/type-intern.{hpp,cpp}`,
  `src/sema/hir-lower-expr.cpp`, and `src/cache/cache-types.hpp`.

## Scope

1. Audit the compile checks in lowering and sharing/diagnostic paths that
   reject `opaque` based on module-local origin.
2. Restore canonical tags from `cache::Artifact::canonical_mappings` during
   hydrate and make `hydrateFromArtifact` apply them before HIR/codegen use.
3. Define the cross-module rule: a bare `opaque` tag is valid when its
   canonical type id was produced by the declaring module or is present in the
   hydrated artifact; do not fabricate a new local random id.
4. Make lowering accept imported/cached `opaque` while keeping diagnostics for
   genuinely invalid module-local opaque uses.
5. Add tests: export `opaque` from module A, import in module B, compile with
   cache hits; plus a cache round-trip test for `opaque` canonical tags.
6. Update `docs/implementation-debt.md` and `memory/` to reflect the closed
   debt or remaining incomplete bits.

Out of scope:

- Changing bare `opaque` runtime storage layout.
- Full-Zith `comptime` or reflection semantics.
- Other cache format changes unrelated to opaque hydration.

## Acceptance Criteria

- Imported `opaque` values no longer fail with the module-local-only error.
- Cached artifacts restore the same canonical tag so `at-canonicalType(T)` and
  `opaque is T` remain deterministic.
- Invalid uses still produce actionable diagnostics.
- Focused cache/cinterop/codegen tests and full CTest pass.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'cache|codegen|cinterop' --output-on-failure
cmake --build {REPO}/build --target fmt-check
ctest --test-dir {REPO}/build --output-on-failure
```

After success:

```bash
cd {REPO}/.awt/{AGENT}
{AWT_SKILL_DIR}/scripts/awt checkin {REPO} "{AGENT}: {TASK}"
{AWT_SKILL_DIR}/scripts/awt request-merge {REPO} "{AGENT}: {TASK}"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
