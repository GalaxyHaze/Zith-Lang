# VM v2 Stdlib Parity

Status: current (2026-10-18).

This document records which stdlib-dependent programs behave the same through
the native `zithc` path and the VM v2 path, and which ones remain excluded
from parity acceptance. It is the human-readable companion to
`tests/test-stdlib-parity.cpp`, which runs the same programs through both
paths and diffs stdout plus exit code. The parity slice is registered in
CTest as `test-stdlib-parity`.

## How the harness works

For each program in the table the harness:

1. Copies the source into a per-case work directory and runs it through
   `zithc --include <stdlib> run`, capturing stdout and the exit code.
2. Runs the same source through `session::CompilationSession` up to
   `HirLowered`, then `vm::lowerModule` and `vm::Vm::runMain`.
3. Compares the two observable behaviors.

Success requires both paths to complete with status 0, identical stdout bytes,
and the same program exit code. When the VM reports unsupported status 5 while
the native path succeeds, an excluded case passes only with a reason naming
its exclusion category. The report prints the observed VM phase and detail
separately. Traps, OOM, output differences, and exit-code differences fail.

The playground status scale mirrors `src/wasm/playground.cpp`: 0 ok, 3 trap,
4 out of memory, 5 unsupported. `vm::RunStatus::Unsupported` maps to 5.

## Passing programs

The VM and the native path agree on stdout and exit code for these programs.

| Program | Verified behavior |
| --- | --- |
| `optional_none_is_null` | None is detected by `is null` |
| `optional_some_presence_and_must` | Some is present and `must` extracts its value |
| `optional_pointer_niche` | None and Some pointer values preserve niche behavior |
| `implicit_numeric_enum` | implicit discriminants preserve current numeric behavior |
| `explicit_integer_enum` | an enum with an explicit `u8` type preserves its value |
| `tagged_union_checks_and_narrowed_casts` | tags, checks, and narrowed casts agree |
| `raw_union_cast_in_raw_context` | raw union construction and cast preserve the payload |
| `dyn_method_dispatch` | dispatch selects the implementation for each dynamic value |
| `opaque_checked_extraction` | type checks and matching/mismatching extraction agree |
| `runtime_pipe_source_once` | the pipe source runs once and its result is preserved |
| `runtime_do_keeps_chain_value` | the effect sees and preserves the current value |
| `runtime_state_transition` | the state transition returns the terminal value |

## Excluded or conditional cases

These cases are diagnostic only because their behavior depends on external
FFI, allocator, or `InPlace` contracts excluded from this parity slice.
Six currently report status 5 during VM lowering with the detail
`unsupported slot size in v2 lowering`. This is the observed blocker, not a
claim that the exclusion category caused that diagnostic.

| Program | Exclusion category | Current observation |
| --- | --- |
| `console_println_literal` | console output and `dyn Allocator` layout | status 5, VM lowering |
| `console_print_no_newline` | console output and `dyn Allocator` layout | status 5, VM lowering |
| `console_println_two_literals` | console output and `dyn Allocator` layout | status 5, VM lowering |
| `console_println_format_int` | console output, allocator, and variadic formatting | status 5, VM lowering |
| `hash_map_u64_put_get` | stdlib hash-map allocator and external FFI contracts | status 5, VM lowering |
| `allocator_heap_dyn_dispatch` | allocator and `dyn Allocator` contracts | status 5, VM lowering |
| `inplace_opaque_contract` | allocator and `InPlace` contracts | currently passes on both paths |

The allow-list accepts only unsupported status 5 for these excluded cases.
Remove an allowance only when a case is in scope and has passing parity
evidence. Keep excluded cases classified even if they currently pass.

## Guarding against silent divergence

The format fast path used to forward only the literal message and drop the
variadic values, so `println("bucket=#", 73)` printed `bucket=#` through the
VM while the native path printed `bucket=73`. `vm::lowerModule` now counts
the placeholders in a literal message slice and reports status 5 when a
message carries placeholders and values. A message without placeholders
still ignores the extra values, matching the native stdlib behavior.

## Running the slice

```bash
cmake --build build --target test-stdlib-parity
ctest --test-dir build -R parity --output-on-failure
```

The test skips with exit code 77 when the build lacks the VM v2 slice, a
`zithc` target, or C interop, because the native path and the shared stdlib
sources are required.
