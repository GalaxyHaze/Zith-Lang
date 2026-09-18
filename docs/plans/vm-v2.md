# VM v2: Typed Execution IR And Linear Memory Runtime

Status: planning

This is the contract for a second runtime path, kept separate from the
existing `src/ir` + `src/interp` execution IR v1 so the shipped
`Zith--` fallback is not destabilized.

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
- No ZIRL format change: ZIRL continues to store HIR, and v2 IR is a lowering
  target produced by the compiler later.
- No browser/WASM packaging in the first milestone.
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

Missing externs trap with `RunStatus::Trap`; OOM and invalid memory accesses
also trap with a clear status.

## Acceptance

- `tests/test-vm-v2.cpp` builds and passes without LLVM.
- The harness runs a manual typed IR main that allocates arena bytes, stores a
  string, calls `malloc`, calls `puts`, computes a value and returns it.
- The same test asserts output, exit code and explicit trap/missing-main
  statuses.
- The existing `test-abi-execution` and execution IR v1 tests remain unchanged.
