# Execution IR And Interpreter Contract

Status: drawing

Size: system, first slice is a single unit.

## Problem

Zith needs a portable execution path that does not depend on LLVM and an IR
that a future tiny backend can consume. The repository currently documents an
Interpreter HIR as the portable path, but there is no executable interpreter
under `src/` and no explicit contract for the IR that a VM would run.

The goal is one small execution IR with two consumers in mind: a simple HIR
interpreter for `--interpreted` and a tiny backend emitter later. The ideas
share the same contract surface and stay in one drawing until the split is
justified.

## Non-Goals

- No LLVM codegen changes.
- No optimizer or SSA lowering in the first slice.
- No full Zith-- surface in the first slice.
- No WASM-specific runtime work beyond reusing the existing playground path.
- No new CLI user surface beyond accepting and executing `--interpreted`.
- No struct layout contract until the layout rows and target ABI are named.

## Open Decisions

Answer each decision directly under the line. The idea becomes promising only
when every decision below has a chosen answer and a named reversal.

1. **Contract surface**: do we define one execution IR contract with both
   consumers, or two contracts now?

   Recommended: one contract, two consumers. Split only when the interpreter
   and tiny backend disagree on IR shape.

2. **Interpreter relationship**: does the HIR interpreter walk HIR directly or
   execute the small IR?

   Recommended: the IR is the interpreter's target. The IR is validated by
   execution before a tiny backend consumes it.

3. **IR shape**: slots with fixed-size values and arena-backed aggregates,
   stack machine, or register/SSA?

   Recommended: slots plus arena. It stays close to the existing HIR slot model
   and is simple enough to lower to a tiny backend later.

4. **Call ABI v1**: ordinary functions and simple extern stubs only, or also
   `state`, `dyn`, `opaque`, and variadic slices?

   Recommended: ordinary functions and simple extern stubs only. The other
   surfaces become separate promises after the execution loop works.

5. **Checks and traps**: where do null, bounds, type id, division, and panic
   checks live?

   Recommended: the compiler proves static rules, and the IR emits explicit
   `trap` instructions for runtime checks that cannot be proven statically.
   The IR does not do hidden semantic dispatch.

6. **Runtime FFI**: which extern functions does the first interpreter support?

   Recommended: only the existing Runtime FFI handler surface, with a table
   entry per linkage name and a clear missing-handler trap.

7. **Repository layout**: do we use `src/ir/` and `src/interp/`, and does
   `--interpreted` become the command that executes this path?

   Recommended: yes to both. `src/vm/` is avoided because the portable runtime
   is an interpreter over IR, not a separate VM module.

8. **First conforming seam**: is the first ABI test a standalone executable
   that runs a small HIR module through IR, or is it the WASM playground?

   Recommended: a standalone test under `tests/` first. WASM reuse comes after
   the host path proves the promises.

9. **Struct layout**: which runtime records need an ABI line in the first
   slice?

   Recommended: the IR instruction header, the function call frame, and the
   runtime FFI entry. Each row names field, offset, size, and alignment after
   the implementation layout is chosen.

## Next Step

Commit this drawing before technical opinions. Then answer the open decisions
in the file, one commit per batch, until the drawing is stable enough to
compute promises.
