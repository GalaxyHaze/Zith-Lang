# VM v2 Dynamic Call Range And Hybrid Dispatch

Status: accepted

## Context

VM v2 was originally introduced with a minimal execution contract supporting up to two
function arguments (`Op::CallFn` accepting only `lhs` and `rhs` registers). Standard library
calls were either unconditionally filtered out during lowering or intercepted via hardcoded
checks (e.g. `startsWith(linkage, "std.io.console.println")` mapping directly to `puts`).

This introduced significant limitations:
1. User-defined functions with 3 or more arguments were rejected with
   `unassigned register in v2 lowering`.
2. `print` (without newline) could not be called, resulting in
   `unsupported resolved call target in v2 lowering (status 5)`.
3. Standard library functions without special-cased hooks could not be executed even when their
   Zith code used constructs fully supported by the VM.
4. Strings were treated as null-terminated C-strings via `puts`, incompatible with arbitrary
   `[]char` slices.

## Decision

### 1. `CallRange` Calling Convention
Instead of expanding fixed operand fields indefinitely in `Instr`, `IR v2` adopts the
`CallRange` register-window calling convention:
- The caller evaluates arguments into a contiguous sequence of local registers:
  `[start_reg .. start_reg + count - 1]`.
- An instruction `Op::CallRange` (or dynamic `CallFn` with `arg_base` and `arg_count`) specifies:
  - destination register `dst` for the return value;
  - callee function table index `imm`;
  - base argument register index `arg_base`;
  - argument count `arg_count`.
- Upon entering the callee frame, the VM directly copies the slice of values
  `regs[arg_base .. arg_base + count]` into the callee parameter registers `calleeRegs[0 .. count - 1]`.

### 2. Hybrid Dispatch for Standard Library
The lowering pass in `src/vm/hir-to-vm.cpp` implements a two-tier hybrid resolution strategy:
1. **Intrinsics / Fast Handlers**:
   Recognized runtime routines (such as `std.io.console.println` and `std.io.console.print`) are
   mapped to efficient VM extern operations.
   - `println` continues to dispatch to `puts` (or `write_stdout` + newline).
   - `print` dispatches to `write_stdout(ptr, len)`.
2. **Fallback to Canonical Zith Code**:
   If no intrinsic matches, the lowering pass no longer skips `std.` or `zith.` modules.
   Instead, it lowers the actual Zith function definitions into VM functions. If the canonical
   code contains unsupported HIR features (such as dynamic traits or unsupported intrinsics),
   the compiler gracefully reports status `5` (`kPlaygroundStatusUnsupported`).

### 3. Exact-Length Standard Output
The VM memory interface exposes an exact-byte write primitive:
- `write_stdout(ptr, len)` reads exactly `len` bytes from linear guest memory and appends them
  directly to `state.output` without requiring a null terminator and without injecting a newline.
- This accurately models the semantics of `print(msg: []char)`.

### 4. Slices as Scalar Pairs
In accordance with Zith's slice layout contract, slices passed to functions and intrinsics
decompose into two scalars: the guest pointer and the length.

## Consequences

- Functions with arbitrary arity can be lowered and invoked without modifying instruction bit-widths.
- Programs in the browser playground can call both `print` and `println` seamlessly.
- Simple standard library utilities that do not use complex dynamic traits execute directly in
  the VM without requiring hand-coded C++ emulation.
- Unsupported standard library code fails with predictable status `5` diagnostics rather than
  silent lowering omissions.
