# Execution IR And Interpreter Contract

Zith defines two portable runtime paths. The HIR interpreter is selected by
`--interpreted` and executes `HirModule` without LLVM codegen or a linked
native binary. The execution IR contract is register-based and shared by the
IR VM and a future tiny backend. The IR VM becomes the default runtime
execution path when LLVM or native codegen is not used.

The source of truth is `docs/plans/abi/execution-ir.md`.

Status: accepted

Promises accepted: `ABI-EXEC-01` through `ABI-EXEC-11`.

## Runtime Distinction

`--interpreted` refers exclusively to the HIR interpreter. It does not select
the execution IR path. The IR VM is the default runtime execution path when
LLVM or native codegen is not used.

## Contract Scope

The execution IR lives under `src/ir/`, the interpreters under `src/interp/`,
and `src/vm/` is not introduced. The first slice covers ordinary functions
and a minimal extern `fn` C subset. The HIR interpreter keeps checks inside
itself; the execution IR expresses checks as explicit trap instructions.
The IR stores metadata only in this slice.

## Conforming Work

The first conforming seam is a standalone hello-world test under `tests/`
that runs the HIR interpreter and verifies program output and exit status.
