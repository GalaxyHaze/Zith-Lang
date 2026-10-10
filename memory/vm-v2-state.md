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
- Each VM function frame restores its `AllocBytes` watermark on return or trap.
  Nested frames preserve caller scratch, and the global heap watermark keeps
  `MallocBytes` allocations alive across frame exits.
- The WASM playground exposes structured JSON diagnostics through
  `zith_last_diagnostics_json_ptr/len`. Compiler diagnostics include severity,
  message, code, and byte-offset span. Runtime and ABI errors include severity
  and message while the existing numeric status codes remain stable.
- The playground implements the compile-once HIR artifact cache. The host calls
  `zith_compile_hir` once, replays with `zith_execute_cached`, and can persist a
  blob and reload it with `zith_restore_cached`. Entries are keyed by source
  bytes plus the stdlib pack fingerprint, and a cache miss or stale blob is
  reported through the diagnostics channel instead of recompiling silently.
- The FFI subset is validated and data-driven in `kValidatedExterns`
  (`src/vm/vm-v2.cpp`): `puts`, `putchar`, `write_stdout`, `malloc`, `calloc`,
  `free`, `realloc`, `snprintf` (`%u`, `%d`, `%g`), the `printf` variadic
  surface, `strlen`, `memcpy`, `strncmp`, and `getchar`. Unsupported externs
  trap with a message naming the missing extern. `getchar` returns -1 because
  the playground has no stdin.
- `realloc` preserves live allocation contents through the allocator's block
  tracking.
- `tests/test-stdlib-parity.cpp` runs stdlib programs through both `zithc` and
  the VM v2 path and diffs stdout plus exit code. A status-5-only divergence is
  allowed only with a documented reason in the case table, so silent output
  drift fails the suite. `vm::lowerModule` rejects a `println`/`print` call
  whose literal message has format placeholders and carries variadic values,
  instead of forwarding only the literal to `puts`/`write_stdout`.

## Quick Checks

- `ctest --test-dir build -R vm-v2 --output-on-failure`
- `ctest --test-dir build -R parity --output-on-failure`
- `ctest --test-dir build -R test-abi-execution --output-on-failure`
- `src/vm/typed-ir.hpp` for the opcodes and register model.
- `memory/execution-ir-drawing.md` for the archived v1 contract status.
