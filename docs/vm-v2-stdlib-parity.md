# VM v2 Stdlib Parity

Status: current (2026-10-10).

This document records which stdlib-dependent programs behave the same through
the native `zithc` path and the VM v2 path, and which ones the VM lowering
still rejects. It is the human-readable companion to
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

When the VM path reports the unsupported playground status (5) while the
native path succeeds, the case is a documented divergence. It passes only
when the program carries an explicit reason in the case table. Any other
divergence, including one where the VM runs to completion but prints
different bytes or returns a different exit code, fails the suite.

The playground status scale mirrors `src/wasm/playground.cpp`: 0 ok, 3 trap,
4 out of memory, 5 unsupported. `vm::RunStatus::Unsupported` maps to 5.

## Passing programs

The VM and the native path agree on stdout and exit code for these programs.

| Program | Behavior |
| --- | --- |
| `console_println_literal` | `println("hello parity")` writes the literal plus newline |
| `console_print_no_newline` | two `print` calls write continuous text without a newline |
| `console_println_two_literals` | `println("a", "b")` writes `a` plus newline and ignores the unused value |

These cover the VM lowering fast path that forwards a literal message to
`puts` (for `println`) or `write_stdout` (for `print`).

## Status-5 programs

The native path runs these programs and the VM lowering rejects them with
status 5. Each carries an allow-list reason tied to the debt inventory.

| Program | Reason |
| --- | --- |
| `console_println_format_int` | `println("bucket=#", 73)` needs variadic `Formatable` rendering, which is outside the VM lowering subset (debt item 1) |
| `hash_map_u64_put_get` | the u64 hash map hits a HIR branch shape the VM lowering does not handle (debt item 1) |
| `allocator_heap_dyn_dispatch` | `std.alloc.allocate` goes through `dyn Allocator` dispatch, outside the VM subset (debt items 1 and 4) |
| `inplace_opaque_contract` | the `InPlace` opaque contract is outside the VM subset (debt item 1) |

The list grows as the lowering surface grows. When a case starts passing,
remove its allow-list entry so a future regression fails the suite.

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
