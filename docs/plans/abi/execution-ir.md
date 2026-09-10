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

## Decisions

These decisions are settled. Each one has a named reversal below because the
next lifecycle step is promising, not another drawing.

1. **One execution IR contract, two consumers**.

   Chosen: a single execution IR contract serves the HIR interpreter and the
   future tiny backend. The contract splits only if the two consumers disagree
   on IR shape.

   Reversal: the tiny backend proves that the interpreter-target IR cannot be
   lowered without losing required semantics.

2. **Two interpreters**.

   Chosen: the project will have two runtime paths. The HIR interpreter is
   simpler and comes first. It serves WASM and builds without LLVM. The
   execution IR interpreter is the later, more optimized VM that also becomes
   the tiny backend input.

   Reversal: the HIR interpreter stays usable but the IR path becomes the only
   portable path.

3. **IR shape: register-based**.

   Chosen: the execution IR uses registers, not slots plus arena and not a
   stack machine. The concrete register model is a promise to compute after
   this drawing.

   Reversal: register lifetime analysis makes the first symmetric slice too
   expensive. Slots become the first runtime model instead.

4. **Call ABI v1: ordinary functions and a small extern subset**.

   Chosen: the first interpreter supports ordinary functions and a minimal set
   of common `extern fn` stubs. `state`, `dyn`, `opaque`, and variadic slices
   are separate promises later.

   Reversal: a hello-world test needs one of those surfaces to run. That
   surface moves into the first slice instead.

5. **Checks live by runtime shape**.

   Chosen: the HIR interpreter keeps checks inside the interpreter. The
   execution IR emits explicit trap instructions because it is register-based
   and must stay literal. The HIR path does not need trap instructions in the
   first slice.

   Reversal: the two paths diverge on a check semantic that one interpreter
   cannot reproduce through its adopted model.

6. **Minimal host runtime is in scope**.

   Chosen: the runtime FFI handler surface is included, but it is small: a
   minimal C subset, no complete libc. The first interpreter owns the handler
   table and reports a clear trap when a linkage name has no handler.

   Reversal: a common extern call proves that the minimal subset cannot stay
   compatible. The handler surface must be reduced instead.

7. **Repository layout: `src/ir/` and `src/interp/`**.

   Chosen: the execution IR lives in `src/ir/`, the interpreters live under
   `src/interp/`, and `--interpreted` executes this path. `src/vm/` is
   avoided as a separate runtime vocabulary.

   Reversal: the interpreter and the compiler need the IR types in a shared
   frontend location. The source layout moves without changing the contract.

8. **First conforming seam: standalone hello-world test**.

   Chosen: the first ABI test is a standalone test under `tests/` that lowers a
   small HIR module, translates or interprets it, and runs a hello-world
   program. WASM playground reuse comes after the host path works.

   Reversal: the standalone host path cannot prove WASM behavior. A WASM test
   joins the first slice instead.

9. **IR stores metadata only**.

   Chosen: the IR stores metadata and contracts. The implementation behind it
   is independent, so a later backend can keep its own internal execution
   engine while accepting the same IR. Layout rows for the IR instruction
   header are not promised in this slice.

   Reversal: an IR consumer needs binary-instruction layout before it can read
   the IR. Layout rows become part of the contract instead.

## Next Step

Compute the first promises from the settled decisions. The immediately
promising areas are the one-contract surface, the HIR-first delivery order,
the register-based IR model, the minimal extern set, the check model, the
`src/ir/` + `src/interp/` layout, and the standalone hello-world seam.
