# {AGENT}: {TASK}

## Goal

Turn the bare `opaque` tag stability path into an explicit cross-module
canonical registry/evolution contract, replacing the current implicit
"invalidar cache" failure mode.

## Context

- Plan: debt, opaque canonical registry and cache evolution.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "Bare opaque usa hydration
  estável mas ainda depende de canonização consistente", and
  `docs/impl-status.md` under `opaque`.
- Canonical tags are deterministic and persisted through
  `canonical_mappings`; `CompilationSession::scanStage()` detects tag
  instability after hydration.

## Scope

1. Do not design a new user-facing type system here. Define a registry
   contract for how a canonical id maps to a persistent project-local runtime
   tag and how the registry evolves when canonical field order changes.
2. Make the current check produce a diagnostic that identifies the canonical id
   and recommends a deterministic recovery path, instead of only the generic
   "invalidate the cache" message, when that is feasible.
3. Add cache tests for cold build, warm hydration, imported opaque values, and
   a simulated canonical-tag divergence.
4. Update debt and status docs with the chosen evolution rule.

Owned files for implementation:

- Cache registry/keying: `src/cache/cache.cpp`, `src/cache/cache.hpp`,
  `src/cache/cache-types.hpp`, `src/cache/cache-entry.cpp`,
  `src/cache/cache-entry.hpp`, `src/cache/artifact-builder.cpp`,
  `src/cache/artifact-builder.hpp`.
- Persistence and hydration: `src/session/persistent-cache.cpp`.
- Canonical tag tables: `src/types/type-intern.cpp`, `src/types/type-intern.hpp`.
- HIR canonical emission only where the registry is created/assigned:
  `src/sema/hir-lower-types.cpp` and `src/sema/hir-lower-expr.cpp`.
- Tests: `tests/test-cache.cpp` and `tests/test-cache-entry.cpp`; do not edit
  `tests/test-hir-lower-modern.cpp` or add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`,
  `memory/flat-containers-cache.md` if the ownership note changes.
  Edit only the section/line for this item in each shared file.

Out of scope:

- Numeric narrowing diagnostics; this task must not edit
  `src/sema/sema-cast-coerce.cpp`.
- Trait/interface import conformance; this task must not edit
  `src/session/frontend-symbol-resolution.cpp` or `src/sema/sema-method.cpp`.
- Nominal construction syntax; this task must not edit
  `src/frontend/ast-lowerer.cpp` or `src/frontend/frontend-decl.cpp`.
- Pipeline orchestration; do not edit `src/session/compilation-session.cpp`.
- HIR cast lowering in `src/sema/hir-lower-expr.cpp`; the range debt task owns
  that file and this task should treat it as read-only reference.

## Acceptance Criteria

- Cross-module `opaque` tags remain deterministic on warm cache paths.
- The explicit registry/evolution rule is documented.
- A divergence test proves the diagnostic is actionable and deterministic.
- No `.zirl` cache regression occurs.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'cache|opaque|hir-lower' --output-on-failure
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
