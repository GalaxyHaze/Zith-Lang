# Monolith Splits Memory

This note records the durable contract and current state for the source-level
monolith splits tracked from `docs/implementation-debt.md`. The full execution
plan lives in `docs/plans/monolith-splits.md`; this memory file keeps the split
facts that are easy to restate incorrectly after a merge.

## Status Summary

Two large session monoliths are merged:

- `src/session/frontend-context.cpp` was split into module analysis, module
  cache, source catalog, symbol resolution, and a smaller orchestration entry
  point.
- `src/session/compilation-session.cpp` was split into native link/run,
  persistent/object cache, pipeline-plan, and a smaller session orchestration
  unit.

The remaining primary candidate is `src/codegen/codegen-emit.cpp`.
`src/sema/hir-lower-expr.cpp` and `src/frontend/frontend-expr.cpp` are
secondary candidates that should not be scheduled until the codegen split is
finished and the remaining counts are rechecked.

## Completed frontend-context Split

The public frontend/session glue remains in
`src/session/frontend-context.cpp`. The current line counts are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `frontend-context.cpp` | 315 | public parsing/frontend orchestration entry point |
| `frontend-module-analysis.cpp` | 412 | module analysis state and discovery |
| `frontend-module-cache.cpp` | 275 | module cache bookkeeping |
| `frontend-source-catalog.cpp` | 196 | source catalog and fingerprinting helpers |
| `frontend-symbol-resolution.cpp` | 764 | import requests and symbol/module resolution |

The old ~1789-line count is intentionally mentioned only as history to signal
that the file was a monolith. When updating debt or plan line counts, re-run
`wc -l` on the TUs above instead of trusting this table forever.

The exact counts came from the `agent5` worktree (2026-09-07). The values are
intended as a baseline for the next task, not as permanent source of truth.
Future split tasks should record their own counts after the relevant branch is
rebased and built.

### Split Responsibilities

The extracted TUs split the old file by pipeline responsibility rather than by
public class boundary:

- `ContentFingerprint`, source catalogs, and cache bookkeeping are separated
  from actual module analysis.
- Import requests, module discovery, and symbol/module resolution stay in the
  symbol-resolution unit.
- Parsing orchestration and the public `FrontendContext` entry point remain
  small enough to scan the pipeline flow in one file.

This layout was chosen so the next frontend change only needs to touch the
unit that owns the behavior. A platform-import variant or a cache-key change
should not force a reader to open the whole frontend session layer.

The platform-import resolver is the clearest example: it was implemented in
the pre-split `frontend-context.cpp` and then moved during this refactor. A
stale plan that still points at the old file should be corrected to
`frontend-symbol-resolution.cpp` rather than copied into the next task.

## Completed compilation-session Split

`CompilationSession` orchestration remains in
`src/session/compilation-session.cpp`. The current line counts are:

| Translation unit | Lines | Responsibility |
|---|---|---|
| `compilation-session.cpp` | 871 | pipeline stage orchestration and session glue |
| `native-link.cpp` | 421 | native link/run helpers |
| `persistent-cache.cpp` | 721 | persistent/object cache helpers |
| `pipeline-plan.cpp` | 13 | planned pipeline stage contract |

The old ~1985-line count is also intentionally historical. The session file now
concentrates the orchestration path; native tool invocation and artifact
persistence are isolated behind focused TUs.

The exact counts from `wc -l` at the same baseline were 871, 421, 721, and 13
for the four session units listed below. Keep these numbers explicit in plan
and debt docs so the completed state reads as merged rather than pending.

### Split Responsibilities

The compilation-session split keeps the highest-level flow readable:

- Pipeline stage order and session-wide state remain in the session TU.
- Link and run actions moved out of the session flow because they are not part
  of the compiler pipeline contract.
- Persistent cache bookkeeping moved out because it changes for a different
  reason than pipeline semantics.
- The tiny `pipeline-plan` TU documents the planned stage set without hiding it
  inside a large implementation file.

There was also an intermediate `pipeline-plan` file in the historical start of
the compilation-session split. The current 13-line file is the final planned
stage contract; do not restore a larger copy as part of a later refactor.

## Remaining Candidates

Current counts from the worktree used for this task:

| File | Lines | Candidate split |
|---|---|---|
| `src/codegen/codegen-emit.cpp` | 1264 | emission by area (params, expr, control flow) |
| `src/sema/hir-lower-expr.cpp` | 2357 | secondary candidate; revisit if it still exceeds ~1000 lines |
| `src/frontend/frontend-expr.cpp` | 1115 | secondary candidate; revisit only if it remains a clear bottleneck |

The primary next merge should be one behavior-preserving extraction from
`codegen-emit.cpp`. It is independent from `drop` feature work and from the
archived full-Zith plans.

### Secondary Candidate Guidance

`hir-lower-expr.cpp` is the largest remaining TU, but it is not automatically a
good split target. The file concentrates expression lowering, which is a
cohesive responsibility even though it is long. Only schedule it after the
codegen split and only when a cleaner boundary exists for a
mechanically-safe extraction.

`frontend-expr.cpp` is similar. It owns expression parsing and precedence and
was already split out of `frontend.cpp`; splitting it again should wait for a
specific responsibility (for example call arguments or postfix chains) that
can be extracted without introducing shared parser state.

## Choice of TU Names

The completed splits used names that match the responsibility, not a generic
`utils` or `helpers` file:

- `frontend-module-analysis.cpp` instead of a second copy of module logic.
- `frontend-module-cache.cpp` instead of cache code inside session glue.
- `frontend-source-catalog.cpp` instead of catalog helpers scattered across
  the frontend.
- `frontend-symbol-resolution.cpp` instead of resolver code in frontend glue.
- `native-link.cpp` and `persistent-cache.cpp` instead of session-dependent
  helpers.

This naming convention makes the next extraction consistent: if codegen-emit
is split, use names such as `codegen-params.cpp`, `codegen-expr.cpp`, or
`codegen-control.cpp` only when each unit has a narrow responsibility.

## Non-Goals

The splits are behavior-preserving. They must not be used as an excuse to
reimplement parser/sema/codegen semantics or to add feature gating. A split
commit should not change a diagnostic string, an error code, a public API, or
an internal symbol name unless the refactor exposes a duplicate/incorrect
helper that would otherwise be copied.

## Hygiene

Re-run CMake after adding a `.cpp` because the source list is globbed. Keep
public headers stable and avoid moving call sites across unrelated APIs.

Verify with focused tests for the affected area and the full CTest suite before
requesting a merge. The project uses Clang's `-Weverything -Werror` in some
builds, so new TUs must not introduce new warnings even when the focused test
passes.

## Line-Count Pitfalls

Project docs previously used approximate line counts (e.g. ~1789 and ~1985)
that became stale quickly. Use exact `wc -l` output when recording a completed
split and keep the old numbers only as explicit historical markers:

```bash
wc -l src/session/frontend-context.cpp \
  src/session/frontend-module-analysis.cpp \
  src/session/frontend-module-cache.cpp \
  src/session/frontend-source-catalog.cpp \
  src/session/frontend-symbol-resolution.cpp
```

The same rule applies to remaining candidates; a task that says "~2140" without
rechecking can mislead the scheduler into over/under-estimating a future merge.

Use absolute paths in links when the repo has worktrees; a link like
`/home/diogo/Zith/src/session/...` is clearer than a relative path that
depends on the current checkout. Keep the same convention in debt and plan
documents.

## Relationship To Other Work

This is a Zith infrastructure track and does not depend on the Zith vs
`Zith--` feature split. Monolith splits can proceed in parallel with `drop`
(Zith--), C toolchain work, or the archived full-Zith comptime plan.

Do not confuse monolith-split work with debt entries that reference the same
files. For example, `compilation-session.cpp` and `hir-lower-expr.cpp` appear
in the bare-`opaque` debt because they own the diagnostics, not because the
monolith split will fix that deficit.

Similarly, the C struct-by-value debt is not a monolith-split work item. It is
a validated-surface question: `impl-status.md` marks C header imports
`Working (validated C)` while the debt entry records that unverified records
are skipped. Keep those two statements aligned when editing plans.

## Stale References

After the session splits, these references changed meaning:

- `docs/plans/platform-imports.md` was moved to
  `docs/plans/archive/platform-imports.old.md`; it is no longer an active plan
  and this note records that move for future curators.
- `docs/implementation-debt.md` must no longer list the session files as
  pending with old large line counts.
- `docs/plans/monolith-splits.md` must show the merged TUs before scheduling
  the next candidate.

When a future curator edits these files, search for old counts first:

```bash
rg -n "~1789|~1985|~2140|platform-imports\.md" docs memory
```

Also scan for generic "common C" wording. The status document changed to
`Working (validated C)` after simple record-by-value validation landed, so a
plan that still says `common C` without the validated-record nuance is stale.

## Memory File Rules

Memory files are short operational notes. If table content grows, move raw
tables into `docs/plans/monolith-splits.md` and keep the operational decisions
here. Do not duplicate the full execution contract.

When a remaining candidate is split, update:

- The line counts in `docs/plans/monolith-splits.md`.
- The debt table in `docs/implementation-debt.md`.
- The status summary and remaining-candidate table in this file.
- `memory/README.md` only if the file's role changes.

## Verification Checklist

Before closing a split task, run:

```bash
wc -l <touched .cpp files>
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If `fmt-check` is available, run it on the touched files. The project does not
require a full formatting pass over unrelated code.

Keep the commit narrowly scoped. A monolith split should not fold in a language
feature, a diagnostic wording change, or a new test that duplicates an
existing suite.
