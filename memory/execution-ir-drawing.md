# Execution IR Drawing Status

The active drawing is `docs/plans/abi/execution-ir.md`. It defines one small
execution IR that can serve both the HIR interpreter and a future tiny
backend.

Status: promising. Draft promises `ABI-EXEC-01` through `ABI-EXEC-10` exist
in the drawing. They are not signed.

## Current Scope

- Portable execution without LLVM through `--interpreted`.
- One execution IR contract and two consumers.
- Two interpreters: the simpler HIR interpreter first, the execution IR VM
  later.
- IR shape is register-based.
- Ordinary functions and a small extern C subset in the first slice.
- The HIR interpreter keeps checks internally. The execution IR uses explicit
  traps.
- `src/ir/` and `src/interp/`, and a standalone hello-world ABI test first.
- The IR stores metadata only. Implementation details stay independent.

## Next Transition

Review the draft promises, then sign them in an ADR. The first conforming test
must carry a signed promise id, so no test source is written before signing
unless the review changes a promise.

## Lifecycle Position

The drawing follows `docs/specs/abi-lifecycle.md`. The next transition is
promising: answer the open decisions and compute promises with named seams,
layout rows, and reversals.

When the idea changes, update `docs/plans/abi/execution-ir.md` before touching
tests or source. This file exists so a later session can find the live pointer
without re-reading the whole conversation.
