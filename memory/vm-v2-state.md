# VM v2 State

Short operational pointer for the VM v2 runtime. The contract and signed
promises `VMV2-01` through `VMV2-06` are owned by `docs/plans/vm-v2.md` and
`docs/adr/0021-vm-v2-portable-execution.md`. This note keeps only the facts
that are easy to get wrong when editing that plan.

## Runtime Selection

- VM v2 (`src/vm/`) is the typed execution IR and portable C++ VM. It is
  compiled into `zithcLib` and selected for WASM execution after HIR lowering.
- `zithc run` selects VM v2 when `--virtual-machine` is passed on any build, and
  automatically when the build has no LLVM (`ZITH_HAS_LLVM` off) or targets
  WASM. A native LLVM build without the flag keeps the link-and-exec path.
- `--interpreted` always selects the HIR interpreter explicitly, independent of
  the VM selection.
- With `-DZITH_BUILD_VM=OFF` the slice is absent and `--virtual-machine` reports
  "this build excludes the VM v2 slice" instead of linking.
- Cache hydration does not restore `decl_id`, so `vm::lowerModule` treats a
  function as user code whenever `blocks` is non-empty. Do not reintroduce a
  `decl_id` guard there, it drops hydrated `main` on a warm cache.
- Execution IR v1 (`src/ir/` + `src/interp/`) is archived under
  `archive/execution-ir-v1/` and is not in the active tree.

## Durable Decisions

- VM v2 is linear and typed, not SSA, with no optimizer in the first shape.
- Aggregates are address-backed in the lowering: struct and pack values live in
  linear memory and their slot register holds the guest address. Struct
  literals allocate and store fields, `HirAssign` between aggregates is a
  `MemCopy`, and a struct-returning function takes a hidden result pointer as
  register 0 (see `Function::returnsAggregate`). The caller allocates the result
  slot and reads it back, so an aggregate-returning callee's `Ret` value is
  unused. Optionals and unions are deliberately not lowered yet; an unhandled
  HIR node now fails the lowering instead of silently dropping a store or
  return.
- Scalar locals are memory-backed too, so `&x` is a real address and a write
  through the resulting pointer is visible to later reads of `x`. A scalar slot
  register holds the storage address; `HirSlotLoad` emits a load, `HirSlotStore`
  emits a store, and `HirSlotAddr`/`HirUnary(Ref)` yield the address directly.
  Slices keep their two-register `{ptr, len}` value and arrays keep a base
  pointer, so both stay register-backed; an array is still pointer-valued, so
  dereferencing a `*[N]T` yields the base pointer and indexing it reads the
  original storage. An array field of a struct is laid out inline and copied by
  `MemCopy` like any other address-valued field. Storage for every memory-backed
  slot is reserved once in a function prologue (not at the HIR `slot_alloca`
  site), so a slot declared inside a loop does not allocate on every iteration.
- A function that returns a slice by value carries the fat pointer in two
  registers: `Function::returnsSlice` marks it, the pointer half is the result
  register, and the length half is the next register. A call site reserves both
  registers, and the slice `HirField` case reads the length half (index 1) back
  as a `Move` of `base+1` instead of going through `FieldPtr`, because a slice
  is a register pair and not memory-backed. `@lengthOf`, `slice.length`, and
  bounds checks all read the length through that field. The `Op::Ret` handler
  propagates the second half through `FunctionReturn::hasAux`/`aux`, and every
  call handler forwards it to the caller. `test-vm-v2` covers it in
  `test_vm_v2_slice_return_by_value`; `vir-cache` bumps the schema to v2 to
  serialize the flag.
- Indexing a `*T` pointer with a runtime index lowers to `IndexLoad` with the
  raw bit (2) set and the element size taken from the pointee, so `p[i]` reads
  `elementSize` bytes at `p + i*elementSize`. This is separate from slice
  indexing, which carries its own length. Covered by
  `test_vm_v2_pointer_dynamic_index`.
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
  surface, `strlen`, `memcpy`, `strncmp`, `strcpy`, `strcat`, `strcmp`,
  `memcmp`, and `getchar`. Unsupported externs trap with a message naming the
  missing extern. `getchar` returns -1 because the playground has no stdin.
  The name allow-list in `src/vm/hir-to-vm.cpp` (`lowerCall`) must list every
  validated extern, or lowering rejects the call before dispatch. `strcpy`,
  `strcat`, `strcmp`, and `memcmp` were added so `soon/string` lowers on v2.
- A `HirRuntimePanic` lowers to one of two terminators: the dedicated
  `Op::NullDeref` for the optional-extraction code `R10003`, and the generic
  `Op::Panic` for every other `R10001+` code, whose `imm` carries the code. The
  runtime maps `Op::Panic` to `state.message = "R<imm>"` and a trap, so
  `@assert(false)`/`@check(false)` outside a test report `R10004`. A code that
  does not fit the 16-bit immediate fails the lowering. `Op::Panic` was
  appended last so existing op byte values stay stable; an older VIR reader
  rejects it (`op > Op::Panic` bound) and re-lowers instead of misreading.
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
