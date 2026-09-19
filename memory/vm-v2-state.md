# VM v2 State

VM v2 (`src/vm/`) is a typed linear execution IR and a portable C++ VM with a
small HIR lowering path (`src/vm/hir-to-vm.*`). It is compiled into
`zithcLib` and used by the CLI only on no-LLVM/WASM builds; `--interpreted`
still selects the HIR interpreter explicitly.

The current no-LLVM/WASM execution path is VM v2 after HIR lowering. LLVM
builds keep the native code path, and execution IR v1 (`src/ir` + `src/interp`)
remains isolated. `docs/plans/vm-v2.md` and
`docs/adr/0021-vm-v2-portable-execution.md` are signed for the host slice;
promises `VMV2-01` through `VMV2-06` are proven in `test-vm-v2`.

## Decided Surface (grill-with-docs)

VM v2 should be a linear, typed assembly-like IR plus a C++ VM, not an SSA
middle IR. It represents `fn` and `extern fn` values as typed references
indexed into a module table, and keeps allocators as stdlib constructs built
on top of primitive VM allocation (`AllocBytes`/`MallocBytes` and `malloc`).

The first end-to-end slice covers the two Hello World paths:

- `from std/io/console` then `println(...)`.
- Manual `extern fn putchar/printf`/standard `stdio.h` usage.

The VM FFI subset must cover at least `malloc`, `free`, `putchar`, and the
`snprintf` formats the stdlib actually uses (`%u`, `%d`, `%g`). A later slice
can add `realloc`, `memcpy`, `strlen`, and `printf` variadic surface.

Useful quick checks:

- `ctest --test-dir build -R vm-v2 --output-on-failure`
- `ctest --test-dir build -R test-abi-execution --output-on-failure`
- `src/vm/typed-ir.hpp` for the opcodes and register model
- `src/vm/vm-v2.cpp` and `src/vm/vm-memory.cpp` for the C++ VM
- `src/vm/hir-to-vm.cpp` for the HIR -> v2 lowering subset
- `src/cli/cmd/run.cpp` to see which runtime is actually selected
- `memory/execution-ir-drawing.md` for the old execution IR contract status
