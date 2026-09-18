# VM v2 Typed Execution IR

Status: proposed

## Context

The current execution IR v1 is register-based but keeps values as opaque
`int64_t` rows and supports only a hello-world subset. The project wants a
portable runtime for WASM first and host no-LLVM later, with a typed IR that
can also serve build-time execution and the self-hosted compiler.

## Decision

Introduce `src/vm/` as a separate VM v2 area with a typed IR, linear memory,
guest-offset pointers, fat pointers, arena/frame allocation policy, and a
small FFI subset. Keep `src/ir` + `src/interp` v1 intact for the current
`Zith--` fallback. The first implementation is a standalone harness test;
compiler lowering and WASM exports are later slices.

## Consequences

- The runtime contract can evolve without risking the shipped fallback.
- ZIRL remains HIR-only for now; v2 IR is produced by a future lowering pass.
- A small local test suite can validate math, loops, memory allocation and
  FFI before any pipeline integration.
- When v2 becomes the no-LLVM/WASM default, the old execution IR v1 can be
  retired through a separate transition.
