# VM v2 State

Short operational pointer for the VM v2 runtime. The contract and signed
promises `VMV2-01` through `VMV2-06` are owned by `docs/plans/vm-v2.md` and
`docs/adr/0021-vm-v2-portable-execution.md`. This note keeps only the facts
that are easy to get wrong when editing that plan.

## Runtime Selection

- VM v2 (`src/vm/`) is the typed execution IR and portable C++ VM. It is
  compiled into `zithcLib` and selected for WASM execution after HIR lowering.
- LLVM builds keep the native code path, and `--interpreted` always selects the
  HIR interpreter explicitly.
- Execution IR v1 (`src/ir/` + `src/interp/`) is archived under
  `archive/execution-ir-v1/` and is not in the active tree.

## Durable Decisions

- VM v2 is linear and typed, not SSA, with no optimizer in the first shape.
- `fn` and `extern fn` are typed references indexed into a module table, not
  raw C function pointers.
- Allocators stay above VM primitives (`AllocBytes`/`MallocBytes`/`malloc`).
- The FFI subset is validated and small: `malloc`, `free`, `putchar`, the
  `snprintf` formats the stdlib uses (`%u`, `%d`, `%g`), `realloc`, `memcpy`,
  `strlen`, and the `printf` variadic surface. Unsupported externs trap.
- `realloc` preserves live allocation contents through the allocator's block
  tracking.

## Quick Checks

- `ctest --test-dir build -R vm-v2 --output-on-failure`
- `ctest --test-dir build -R test-abi-execution --output-on-failure`
- `src/vm/typed-ir.hpp` for the opcodes and register model.
- `memory/execution-ir-drawing.md` for the archived v1 contract status.
