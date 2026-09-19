# VM v2 Drawing

VM v2 is the typed, linear assembly-like execution IR and C++ runtime for
portable/browser execution. The host slice is signed through
`docs/plans/vm-v2.md` and `docs/adr/0021-vm-v2-portable-execution.md`; this
page keeps the scoped decision history after `grill-with-docs`.

## Scope Decided

- IR v2 represents `fn` and `extern fn` values as typed references that index
  into the module function/extern table, not raw C function pointers.
- The IR stays linear and typed, with explicit arithmetic/control/memory
  opcodes. SSA, rich phi nodes, or an optimizer are not in the first shape.
- Allocators are stdlib/runtime constructs on top of VM primitives
  (`AllocBytes`/`MallocBytes` and `malloc`), not IR operators.
- `extern fn` FFI is a small validated subset, not full libc.

## First Acceptance Slice

Both Hello World paths now run through VM v2 on host:

- `from std/io/console` and `println("...")`.
- Manual `extern fn putchar`/printf-style output (small validated surface).

The console/format stdlib currently needs `malloc`, `free`, `putchar`,
`snprintf` (`%u`, `%d`, `%g`), `memcpy`, and `strlen` for basic output.
`sscanf`/`strncmp` are needed only when `input()`/`ParseInput` is exercised.
`realloc` is required by `std/alloc`, not by the Hello World slice.

## Backlog Surface

- Full `printf` variadic formatting surface is not required by the stdlib
  console path; `snprintf` scoped to `%u`, `%d`, `%g` is the first FFI sink.
- State machines, `dyn` calls, `opaque`, closures/captures, variadic slices,
  and C header import remain later slices.
- Browser/WASM output remains the existing `host_write` seam, outside the
  first host VM acceptance slice.
