# C Globals in Header Interop

External C globals are imported through the native libclang header path. This
note records the supported slice and its backend boundary for work on
`src/cinterop/c-header.cpp`, foreign symbol resolution, HIR lowering, and LLVM
code generation.

## C Objects and Identity Macros

Some C libraries expose global objects alongside macros with the same name.
For example, libc headers can make `stdin`, `stdout`, and `stderr` available as
`FILE *` objects. An identity macro such as `#define stdout stdout` does not
carry the object's value. The importer must retain the `VarDecl` for the
external object and its C linker name.

The `FILE` name is a typedef, not a complete object layout needed by Zith when
the value is used through a pointer. The importer brings in typedef names
reachable from supported global types without importing unrelated declarations
from system headers.

## Supported Surface

The C header importer accepts supported external scalar and pointer objects
that are declared directly in the requested header. It excludes `static`
objects and declarations that appear only in transitively included headers.
Unsupported objects, such as by-value aggregates outside the validated ABI
slice, receive a skip diagnostic while other supported declarations remain
available.

Top-level object constness controls the binding's mutability. A declaration
such as `extern const int *pointee_const` describes a mutable pointer object
whose pointee is const. A declaration such as `extern int *const object_const`
describes a read-only pointer object. This distinction is preserved when the
header is lowered into Zith types.

Manual module-level declarations use the same binding rules:

```zith
extern var counter: i32;
extern const rate: f64;
extern var local_counter: i32 = extern c_counter;
```

`extern var` permits writes. `extern const` is read-only runtime storage, not a
compile-time constant. `= extern CSymbol` selects a linker symbol that differs
from the source-level name. External declarations require an explicit type,
reject value initializers, and cannot appear inside function bodies.

The compiler resolves imported and manual declarations through the foreign
global identity, lowers referenced objects to `HirExternalGlobalAddr`, and
preserves the exact C symbol for LLVM. LLVM emits an external global declaration
and loads or stores the object according to the binding's mutability. Repeated
references to the same object reuse one LLVM declaration.

## VM v2 Boundary

VM v2 does not implement external global storage. Type checking and HIR lowering
can accept the declaration, but VM lowering reports
`unsupported external global '<name>' in v2 VM` when program code references
it. An unused external declaration does not block VM lowering or execution.

Tests cover imported scalar and pointer globals, reachable `FILE` typedefs,
direct-header filtering, C constness, manual symbol aliases, LLVM reads and
writes, repeated LLVM references, `stdout` through `stdio.h`, and the VM
diagnostic and unused-import cases. Relevant suites include `test-cinterop`,
`test-frontend-modern-pipeline`, `test-codegen`, and `test-vm-v2`.

## Related C APIs

Functions such as `fopen`, `fprintf`, and `fclose` remain ordinary imported
functions. `fopen` and `fprintf` can use `FILE *` values independently of the
global objects. `fdopen` is POSIX and may not be declared by `stdio.h` under a
strict C standard mode, so a program may need a manual `extern fn` declaration.

The `write` function from `unistd.h` can write directly to file descriptors 1
and 2. That remains useful when a program does not need a C `FILE *` stream.
These function APIs do not change the VM v2 limitation for external globals.
