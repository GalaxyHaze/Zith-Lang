# Monolith Splits Memory

This note records the durable contract for the source-level monolith splits
planned from `docs/implementation-debt.md`. The full plan lives in
`docs/plans/monolith-splits.md`.

## Current Priority

`frontend-context.cpp`, then `compilation-session.cpp`, then
`codegen-emit.cpp`. These files remain the largest remaining single-TU
ownership and pipeline responsibilities.

## Non-Goal

The splits are behavior-preserving. They must not be used as an excuse to
reimplement parser/sema/codegen semantics or to add feature gating.

## Hygiene

- Re-run CMake after adding a `.cpp` because the source list is globbed.
- Keep public headers stable and avoid moving call sites across unrelated APIs.
- Verify with focused tests and the full CTest suite before closing.
