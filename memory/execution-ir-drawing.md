# Execution IR Contract Status

The active contract is `docs/plans/abi/execution-ir.md`. It defines one small
execution IR that can serve the IR VM and a future tiny backend, plus a
separate simpler HIR interpreter for explicit `--interpreted` execution.

Status: signed. `ABI-EXEC-01` through `ABI-EXEC-10` are accepted by
`docs/adr/0018-execution-ir-interpreter-contract.md`.

## Current Scope

- Portable execution without LLVM through `--interpreted` for HIR.
- One execution IR contract with two consumers: the IR VM and tiny backend.
- Two interpreters: the simpler HIR interpreter first, the execution IR VM
  later.
- IR shape is register-based.
- Ordinary functions and a small extern C subset in the first slice.
- The HIR interpreter keeps checks internally. The execution IR uses explicit
  traps.
- `src/ir/` and `src/interp/`. `--interpreted` selects HIR only. The IR/VM is
  the default execution path when LLVM or native codegen is not used.
- A standalone hello-world ABI test is the first conforming seam.
- The IR stores metadata only. Implementation details stay independent.

## Next Transition

The first conforming slice is now implemented: `tests/test-abi-execution.cpp`
covers `ABI-EXEC-09`, and `src/interp/hir-interpreter.cpp` backs the
`--interpreted` path from `src/cli/cmd/run.cpp`. The next slice is the IR/VM
execution path; it becomes the default portable runtime when LLVM or native
codegen is not used.

## Lifecycle Position

The plan follows `docs/specs/abi-lifecycle.md`. The next transition is
conforming: write the first contract test, then implement the HIR interpreter
against it.

When the idea changes, update `docs/plans/abi/execution-ir.md` before touching
tests or source. This file exists so a later session can find the live pointer
without re-reading the whole conversation.
