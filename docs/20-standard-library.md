## 20. Standard Library

> **Implementation status:** `stdlib/c/io.zith`, `stdlib/std/io/console.zith`, and
> `stdlib/std/alloc.zith` are the shipped modules. `puts`, `println`, and raw allocator
> primitives work. `stdlib/std/collections/hash_map_u64.zith` ships a concrete
> `u64 -> u64` hash map and `stdlib/std/collections/hash_map.zith` is a checked
> generic `HashMap<K, V>` module. `stdlib/std/new.zith` is a draft that now
> passes `zithc check`; the `InPlace` trait shape and an imported conforming
> `implement Box as InPlace` are covered by `tests/test-generic-hashmap.cpp`.
> The generic `new`/`delete`/`make`/`release` helpers are not usable yet, so the
> module is still marked proposed. All other standard
> library content is **spec-only**. See [impl-status.md](impl-status.md).

`std`/`soon` remain documentation-only in this iteration. No existing module is being rewritten.
The documented convention uses resource types with `init`/`destroy`, read-only methods with
`view`, and mutating methods with `lend`. `defer` runs `destroy(self: lend Self)` on resource
cleanup. `drop` remains outside the `Zith--` subset.

### 20.1 Three-Part Structure

| Namespace | Stability | Use when |
|---|---|---|
| `std` | Stable, backward-compatible | You need a guaranteed API |
| `soon` | Experimental, may change | You're prototyping and don't mind breakage |
| `c` | Direct C FFI bindings | You need to call C APIs |

```zith
import std;
import soon;   // use with caution — API may shift
import c;       // raw C bindings
```

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
`stdlib/std/new.zith` carries the target trait and helper signatures, but it is
marked proposed because the compiler cannot yet instantiate generics that only
appear in the return type or dispatch opaque packs during construction. The
`InPlace` trait itself is checked and covered by a conforming type in
`tests/test-generic-hashmap.cpp`.

#### `std/new` (proposed draft)

```zith
pub trait InPlace {
    fn inplace(var self, allocator: dyn Allocator, args: opaque): bool;
    fn clean(var self, allocator: dyn Allocator) {}
}

pub fn new<T: InPlace>(args: opaque): ?*T;
pub fn delete<T: InPlace>(ptr: *T);

pub fn make<T: InPlace>(allocator: dyn Allocator, args: opaque): ?*T;
pub fn release<T: InPlace>(allocator: dyn Allocator, ptr: *T);
```

This module is intentionally not wired into `test-examples` until the
generic helper API is usable. `examples/inplace-simple.zith` demonstrates the
`InPlace` shape with a local trait and `dyn Allocator`; the module stays as the
single stdlib location for the ADR contract so the larger ownership/allocator
work has a concrete target.

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
