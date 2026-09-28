# C Runtime Standard Library Surface

Status: accepted

## Context

The standard library needs common low-level operations on native LLVM, no-LLVM,
and WASM targets. Requiring every target to parse host `.h` files makes the
standard library depend on libclang, a host sysroot, or a WASM header bundle.
It also conflates two different use cases: common APIs maintained by Zith and
target-specific or third-party C interop.

## Decision

The root `c` namespace is the supported C/runtime surface of the Zith standard
library. Modules such as `c/io`, `c/stdlib`, and `c/string` contain validated
`extern` declarations for common operations used by the standard library.

The declarations define a stable Zith-facing ABI contract. The selected backend
chooses the concrete implementation:

- native LLVM may resolve the declaration through the platform libc;
- the no-LLVM VM may resolve it through VM intrinsics or registered externs;
- WASM may resolve it through a runtime or host import.

The compiler must not require header parsing to consume the supported `c`
surface. `import "file.h"` remains available for platform-specific declarations
and external C libraries that are not part of the supported surface.

Side-effecting functions whose status return is normally ignored may carry
`#[discardable]`. This suppresses the discarded-result diagnostic while
preserving the call and its ABI.

The idiomatic `std` namespace remains above this layer. For example,
`std/io/console` provides `print` and `println`, while `c/io` provides the
lower-level C/runtime operations used to implement them.

## Consequences

- Common standard-library modules no longer need to import `stdio.h`,
  `stdlib.h`, or `string.h` directly.
- No-LLVM and WASM builds can use the same stdlib source without pretending
  that a host libc or header parser exists.
- The `c` surface must stay deliberately small and target-validated.
- Adding a declaration to `c/...` requires a backend implementation or an
  explicit capability/error path for every supported target.
- Header imports remain important for SDL, POSIX extensions, vendor SDKs, and
  other APIs that are not portable enough to belong to `c/...`.
