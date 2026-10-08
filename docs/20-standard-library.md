## 20. Standard Library

> **Status:** see [impl-status.md](impl-status.md) for the current status of each module. The
> shipped modules are `stdlib/c/io.zith`, `stdlib/c/stdlib.zith`, `stdlib/c/string.zith`,
> `stdlib/std/io/console.zith`, the `std/memory` DAG, `stdlib/std/collections/hash_map_u64.zith`
> (a concrete `u64 -> u64` map), and `stdlib/std/collections/hash_map.zith` (a checked generic
> `HashMap<K, V>`). `std/alloc` and `std/new` remain as legacy compatibility modules.

`std`/`soon` remain documentation-only in this iteration, except for the shipped modules listed
above. The common C bindings used by those modules live under `c/` rather than importing host
headers directly.
The documented convention uses resource types with `init`/`destroy`, read-only methods with
`view`, and mutating methods with `lend`. `defer` runs `destroy(self: lend Self)` on resource
cleanup. `drop` remains outside the `Zith--` subset.

### 20.1 Three-Part Structure

| Namespace | Stability | Use when |
|---|---|---|
| `std` | Stable, backward-compatible | You need a guaranteed API |
| `soon` | Experimental, may change | You're prototyping and don't mind breakage |
| `c` | Supported C/runtime surface | You need common low-level or system APIs |

```zith
import std;
import soon;   // use with caution — API may shift
import c;       // supported C/runtime surface
```

The `c` namespace is not a request to parse a host header. It is the stable
low-level surface maintained by the compiler/runtime. Its declarations may map
to libc on a native LLVM target, to VM intrinsics on the no-LLVM backend, or to
host/runtime imports on WASM. The source API stays the same while the selected
backend chooses the concrete implementation.

Use `c/...` for common C-shaped facilities that Zith supports across targets:

```zith
from c/io
from c/stdlib

fn main() {
    puts("hello");
    let memory = malloc(64);
    free(memory);
}
```

Use `import "file.h"` for a target-specific declaration set, a platform API,
or an external C library that is not part of the supported `c` surface. Header
imports remain an interop escape hatch, not the mechanism used to bootstrap the
standard library.

### 20.2 Core Modules

#### `std/io/console`
```zith
fn println(msg: []char): void;
fn print(msg: []char): void;
fn eprint(msg: []char): void;
```

The console module also provides `input()` as the owning line-input
constructor. `InputLine` owns its buffer and exposes `text`, `len`, `good`,
`cast<T>`, and `destroy`. Callers must destroy the line after use.

`ParseInput` is the parsing contract implemented by the primitive numeric and
boolean types shipped with the standard library:

```zith
pub trait ParseInput {
    fn parse(self: view InputLine): ?Self;
}
```

`InputLine.cast<T: ParseInput>` returns `?T` without destroying the line.
Passing `view line` preserves the owner for later use:

```zith
let n = line.cast<i32>();
```

`i32`, `bool`, `f32`, `f64`, and `u32` implement `ParseInput` in the current
stdlib surface. `*char` parsing remains out of scope.

```zith
@println("hello");
```

#### `std/alloc`

Raw storage primitives and a default heap allocator. The trait methods use
`self` read-only receivers because the current compiler invalidates a concrete
receiver after a trait method call in the same scope. The `dyn Allocator`
free functions are the supported repeatable call path.

```zith
pub trait Allocator {
    fn alloc(self, size: u64, align: u64): ?raw opaque;
    fn free(self, mem: raw opaque, size: u64, align: u64);
    fn realloc(self, old: raw opaque, old_size: u64, old_align: u64,
               new_size: u64, new_align: u64): ?raw opaque;
}

pub struct HeapAllocator {}

pub fn allocate(self: dyn Allocator, size: u64, align: u64): ?raw opaque;
pub fn deallocate(self: dyn Allocator, mem: raw opaque, size: u64, align: u64);
pub fn reallocate(self: dyn Allocator, old: raw opaque, old_size: u64,
                  old_align: u64, new_size: u64, new_align: u64): ?raw opaque;
```

`allocate` returns `null` when the underlying `malloc`/`realloc` call fails.
The caller owns the storage and must pass the same `size`/`align` to
`deallocate`. The larger `InPlace`/`new`/`delete`/`make`/`release` contract is
recorded in [ADR 0010](adr/0010-allocator-inplace-drop.md). A draft module at
`stdlib/std/memory/new.zith` carries the target helper signatures, and
`stdlib/std/new.zith` remains as legacy compatibility. The helpers are marked
proposed because the compiler cannot yet instantiate generics that only appear
in the return type or dispatch opaque packs during construction. The `InPlace`
trait itself is checked and covered by a conforming type in
`tests/test-generic-hashmap.cpp`.

#### `std/memory` (target DAG)

```zith
pub trait InPlace {
    fn inplace(var self, args: opaque): bool;
    fn clean(var self) {}
}

pub fn new<T: InPlace>(args: opaque): ?*T;
pub fn delete<T: InPlace>(ptr: *T);
```

`make`/`release` live on `Allocator` as default methods so the convenient
pair follows the allocator model: `allocator.make<T>(args)` and
`allocator.release<T>(ptr)`. They are declared as defaults until layout
queries and opaque pack dispatch are usable inside trait methods. The module
is intentionally not wired into `test-examples` until the generic helper API
is usable; `examples/inplace-simple.zith` continues to demonstrate the local
`InPlace` shape.

The layer DAG is kept strict: `in-place` defines `InPlace` only,
`allocators/allocator` depends on `in-place` and owns `Allocator`,
`allocators/heap` implements `HeapAllocator`, and `new` depends on `in-place`
plus `allocators/heap` without naming `make`/`release`. `std/memory`
re-exports the same-level contracts, including `Allocator`, `HeapAllocator`,
the raw free-function bridge, and `new`/`delete`, so callers can use one stable
import root (`from std/memory`).

`export` paths that share a namespace prefix (for example the `std/...` roots
inside the facade) are deduplicated into one qualified namespace; the public
symbols of every export are injected independently. This keeps a facade usable
even when it re-exports more than one module under the same top-level segment.

#### `std/collections/DynArray`

```zith
struct DynArray<T> {
    fn push(self: lend, val: T);
    fn pop(self): ?T;
    fn len(self): u64;
    fn get(self, index: u64): ?T;
}
```

#### `std/collections/hash_map_u64` (concrete)

The shipped concrete map stores `u64` keys and `u64` values. It uses an
open-addressed linked chain and owns its backing storage with `calloc`/`free`.
Callers must call `destroy` after use.

```zith
import std/collections/hash_map_u64 as hm;

var map = hm.HashMap { count: 0u64, capacity: 0u64, table: null, head: 0u64 };
if not(hm.HashMap.reserve(lend map, 64u64)) { /* allocation failed */ }
hm.HashMap.put(lend map, 1u64, 42u64);
let value = hm.HashMap.get(view map, 1u64); // ?u64
hm.HashMap.destroy(lend map);
```

#### `std/collections/hash_map` (checked generic)

`HashMap<K, V>`, `Entry<K, V>`, and `Hashable` form the generic checked map
module. Concrete generic instances are reified with their type arguments
stored as structural metadata on `StructType`, so nested `Entry<K, V>` slots
keep the correct `K`/`V` mapping and `current.key.hash()` resolves through the
`K: Hashable` bound. The module passes `zithc check` and is covered by
`tests/test-generic-hashmap.cpp`. For a concrete shipped runtime example,
prefer the `u64 -> u64` module above.

#### `std/fs`
```zith
struct File { ... }

fn open(path: string): File!;
fn read(self: view File): []u8!;
fn write(self: lend File, data: []u8): void!;
```

### 20.3 Common Traits

| Trait | What it enables |
|---|---|
| `Copy` | Bitwise copy — primitives and components get this by default |
| `Clone` | `fn clone(self): Self!` |
| `Lent` | Can appear as a `lend` parameter |
| `Share` | Safe to share across threads |

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
