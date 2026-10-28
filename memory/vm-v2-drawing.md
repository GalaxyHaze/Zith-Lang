# VM v2 Drawing

Short operational pointer for the scoped decisions taken during the VM v2
`grill-with-docs` pass. The contract and the signed promises `VMV2-01` through
`VMV2-06` are owned by `docs/plans/vm-v2.md` and
`docs/adr/0021-vm-v2-portable-execution.md`. This note keeps only the boundary
decisions that must not be reopened casually.

## Scope Decided

- IR v2 represents `fn` and `extern fn` values as typed references into the
  module function/extern table, not raw C function pointers.
- The IR stays linear and typed with explicit arithmetic, control, and memory
  opcodes. SSA, rich phi nodes, and an optimizer are not in the first shape.
- Allocators are stdlib/runtime constructs on top of VM primitives
  (`AllocBytes`/`MallocBytes` and `malloc`), not IR operators.
- `extern fn` FFI is a small validated subset, not full libc.

## Floating-Point Register Semantics

VM v2 stores register payloads in `int64_t` slots. An `f64` payload is its bit
representation, copied with `memcpy`; arithmetic, negation, and comparisons
must dispatch using the register's `ValueType`, not perform integer
operations on that payload. The VM semantic version is 3, and
`tests/test-vm-v2.cpp` covers `f64` addition, subtraction, multiplication,
division, negation, and comparisons.

## Backlog Surface

- Full `printf` variadic formatting is not required by the stdlib console
  path. `snprintf` scoped to `%u`, `%d`, `%g` is the first FFI sink.
- State machines, `dyn` calls, `opaque`, closures/captures, and C header
  import remain later slices. Variadic slice tails use pointer/length register
  pairs and are covered by the VM v2 lowering tests.
- Browser/WASM output stays on the existing `host_write` seam, outside the
  first host VM acceptance slice.
