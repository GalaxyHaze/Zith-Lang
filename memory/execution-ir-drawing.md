# Execution IR Drawing Status

The active drawing is `docs/plans/abi/execution-ir.md`. It defines one small
execution IR that can serve both the HIR interpreter and a future tiny
backend.

Status: drawing. No promises, no ADR, no conforming tests yet.

## Current Scope

- Portable execution without LLVM through `--interpreted`.
- An IR shaped for a simple VM first and a tiny backend later.
- No optimizer, no full Zith-- surface, no WASM-specific runtime.

## Lifecycle Position

The drawing follows `docs/specs/abi-lifecycle.md`. The next transition is
promising: answer the open decisions and compute promises with named seams,
layout rows, and reversals.

When the idea changes, update `docs/plans/abi/execution-ir.md` before touching
tests or source. This file exists so a later session can find the live pointer
without re-reading the whole conversation.
