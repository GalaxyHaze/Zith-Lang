## 18. C Interop

Zith offers three modes of C interop, designed to make using existing C libraries as frictionless as possible.

### 18.1 Automatic Binding via `.h`

Including a C header automatically generates bindings. Every function becomes a `raw fn` by default, and pointer types are inferred:

```zith
import "openssl/ssl.h";

// All C functions are now available as raw fn
SSL_CTX_new(method);
```

| C type | Inferred Zith type |
|---|---|
| `T*` | `mut *T` (mutable pointer) |
| `const T*` | `*T` (read-only pointer) |
| `T**` | `mut *mut *T` |
| `void*` | `raw opaque` |
| `int`, `float`, etc. | direct primitive equivalents |

### 18.2 Manual Binding with Semantic Annotation

Override or supplement auto-generated bindings to attach Zith-specific semantics:

```zith
// Equivalent declarations — malloc is a C function (no namespace)
// bindToC is subject to Zith namespace rules
fn bindToC = extern 'C' malloc(size: u64): unique opaque;
extern 'C' malloc(size: u64): unique opaque;   // same thing, no namespace alias
```

### 18.3 External (No Header)

For assembly routines, headerless libraries, or code deliberately outside the project:

```zith
// The linker resolves this; the compiler has no information about the function
fn bindTo = extern someAsmRoutine(x: u64): u64;
```

### 18.4 Exposing Zith to C

```zith
extern 'C' fn my_function(x: i32): i32 {
    x * 2
}
// Generates a C-compatible symbol, callable from C as an ordinary function
```

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
