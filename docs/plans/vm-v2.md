# VM v2: Typed Execution IR And Linear Memory Runtime

Status: conforming (host)

This is the contract for a second runtime path, kept separate from the
existing `src/ir` + `src/interp` execution IR v1 so the shipped `Zith--`
fallback is not destabilized. The signed ADR is
`docs/adr/0021-vm-v2-portable-execution.md`; the host slice is conforming and
the sources now live in `zithcLib`, but the browser/WASM packaging is still
later work.

## Goals

- Run Zith programs in WASM through a typed execution IR and a C++ VM.
- Reuse the same runtime for host builds without LLVM.
- Provide the execution engine that future build-time functions and the
  self-hosted compiler will consume.
- Keep the first slice small and testable without browser glue or a real
  `HIR -> IR` lowering.

## Non-Goals

- No changes to the current execution IR v1 (`src/ir`, `src/interp`) in this
  slice.
The VM v2 lowering lives in `src/vm/hir-to-vm.*` and is wired into the CLI
only on no-LLVM/WASM builds for now. LLVM builds keep the native path, so the
host runtime change does not affect the shipped codegen backend.

- No browser/WASM packaging in this milestone.
- No full language surface: state machines, dyn dispatch, opaque and variadic
  slices are later slices.
- No comptime pipeline yet: build-time functions are a separate workload on
  the same VM once the IR and runtime exist.

## Decisions

- The VM is C++ and uses a typed register IR with a computed-goto execution
  loop once the opcode set stabilizes.
- All IR pointers are guest offsets into one linear memory buffer. Host
  pointers never leak into the IR.
- Structs and slices live by address. The IR has `FieldPtr`, memory load/store,
  `MemCopy`, and fat pointer construction/access.
- A frame creates a checkpoint in the shared arena policy; `Ret` releases the
  frame checkpoint. `AllocBytes` serves frame-local arena allocation and
  `MallocBytes`/`malloc` serves global heap allocations.
- The FFI is a small libc subset implemented by the VM over minimal JS host
  imports when running in WASM.
- The first proof is a local C++ harness that builds the typed IR manually;
  pipeline integration into HIR produces IR after the harness is stable.

## Initial IR Slice

| Group | Opcodes |
| --- | --- |
| Constants | `LoadConstI32`, `LoadConstI64`, `LoadConstF32`, `LoadConstF64`, `LoadString` |
| Memory | `AllocBytes`, `MallocBytes`, `StoreBytes`, `LoadBytes`, `MemCopy` |
| Aggregate/pointer | `FieldPtr`, `MakeSlice`, `SlicePtr`, `SliceLen` |
| Arithmetic | `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg`, `Not` |
| Compare | `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge` |
| Control | `CallFn`, `CallExtern`, `Ret`, `Branch`, `Jump`, `Trap` |

The first slice represents values through typed registers, but the host VM
keeps the execution value width fixed to `int64_t`/bits for primitive values.
Fat pointers use two consecutive registers: pointer guest offset and length.

## Memory Model

`LinearMemory` owns a byte vector. The current bump and heap pointers start at
the beginning of the same buffer. The first milestone does not reclaim arena
blocks per frame; instead the arena checkpoint is reserved for the future
layout that `Ret` will release. The allocation instructions already separate
frame-local scratch from global `malloc`.

WASM moves this buffer to the module linear memory and host FFI converts guest
offsets only at the boundary.

## FFI Subset

The initial host VM supports:

- `puts(ptr)` writes a C string and newline.
- `putchar(ch)` writes one byte.
- `malloc(size)` returns an offset into linear memory.
- `free(ptr)` releases a heap slot.
- `snprintf(buf, size, fmt, u/d)` writes the small scoped formats.
- `strlen(ptr)` and `memcpy` support the console path.

Missing externs trap with `RunStatus::Trap`; OOM and invalid memory accesses
also trap with a clear status.

## Acceptance

- `tests/test-vm-v2.cpp` builds and passes without LLVM and through the
  library build.
- The harness runs a manual typed IR main that allocates arena bytes, stores a
  string, calls `malloc`, calls `puts`, computes a value and returns it.
- The same test asserts output, exit code and explicit trap/missing-main
  statuses.
- The existing `test-abi-execution` and execution IR v1 tests remain passing.
- A no-LLVM CLI build runs a Hello World through VM v2.

## Promises (signed)

- `VMV2-01`: IR v2 is linear and typed; no SSA/optimizer shape in the first
  slice.
- `VMV2-02`: `fn` and `extern fn` use indexed module references, not raw C
  function pointers.
- `VMV2-03`: allocators stay above VM primitives; `std/alloc`/`HeapAllocator`
  run through `AllocBytes`/`MallocBytes`/`malloc`.
- `VMV2-04`: VM FFI is a validated small subset (`malloc`, `free`,
  `putchar`, scoped `snprintf`); unsupported externs trap.
- `VMV2-05`: first end-to-end acceptance runs both console and manual
  `extern fn` Hello World paths and compares observable output/exit code.
- `VMV2-06`: VM v2 stays separate from execution IR v1, but the host slice is
  promoted into `zithcLib` and selected as the no-LLVM/WASM CLI runtime.
