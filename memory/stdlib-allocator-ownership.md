# Stdlib Allocator and Ownership Contract

The target stdlib-only allocator surface is `stdlib/std/memory*`. The strict
module graph is `in-place` -> `allocator` -> `heap` and `new` depends on
`in-place` plus `heap`. The original `stdlib/std/alloc.zith` and
`stdlib/std/new.zith` remain as legacy compatibility modules. The compiler
supports trait-based dynamic dispatch through `dyn Allocator` free-function
helpers, but concrete trait method calls still invalidate the receiver after a
`self`/`var self` call. The surface keeps raw storage primitives, a heap
allocator, and free functions that go through `dyn`.

## Compiler Constraints That Shaped the API

`var self` on `Allocator` methods made a concrete `HeapAllocator` unusable for
more than one call: `let mem = h.alloc(...); h.free(...)` reported `E4001
cannot use 'h' after it was moved by a previous call`. `lend`/`view` receivers
did not match the trait requirements and produced `E2022` signature errors.
`self` without an explicit qualifier works as a read-only receiver and is the
shape used by `Formatable` elsewhere in the stdlib.

Calling a method on `dyn Allocator` inside a free function works repeatedly:

```zith
fn allocate(self: dyn Allocator, size: u64, align: u64): ?raw opaque {
    return self.alloc(size, align);
}
```

Calling `h.alloc(...)` twice on a concrete `HeapAllocator` in the same scope
currently fails with `E4001` even with read-only trait receivers. Users should
go through `std.memory.allocate` and `std.memory.deallocate` for now.

## Module Resolution Quirk

The legacy `std/alloc` failed with `E2001 unknown struct type` while `std/alloc3` with
identical content imported correctly. The empty directory
`stdlib/std/alloc/` was the cause: the resolver treated the import as a
directory module. Removing the empty directory and clearing the stale
`.zith-cache` directories restored `from std/alloc` and
`import std/alloc as a`.

`export` paths in a facade may share a namespace prefix (`export std/a` plus
`export std/b`). The frontend injects each target's public symbols and keeps an
independent `ModuleAlias` for every exported namespace prefix, so `from
std/memory` exposes the leaf contracts and `import std/memory` can resolve each
fanout path such as `std.memory.in-place.InPlace` without a spurious `E2002
duplicate binding`.

## Design Intent

The target proposal in `docs/adr/0010-allocator-inplace-drop.md` separates:

- `Allocator`: storage primitives `alloc`/`free`/`realloc` with `?raw opaque`.
- `InPlace`: object construction/cleanup hooks for allocator-based values. The
  trait does not name an allocator, so the module graph stays DAG-shaped. An
  imported `implement Box as InPlace` with qualified trait calls is covered by
  `tests/test-generic-hashmap.cpp`.
- `new<T>`/`delete<T>` and `make<T>`/`release<T>`: heap and generic allocator
  convenience pairs. `stdlib/std/memory/new.zith` carries the target helper
  signatures as a proposed draft because the helpers still cannot be
  instantiated. `stdlib/std/new.zith` remains as compatibility.
- `W10xx DiscardedValue` and `W11xx DiscardedOwner`: future compiler warnings;
  no codegen changes are shipped in the first stdlib-only step.

## `std/memory/new.zith` Compiler Gaps

The API draft in `stdlib/std/memory/new.zith` is the target single source for
the `InPlace`/`new`/`delete` contract. The module and the `InPlace` trait itself
now pass `zithc check`; the helper generics remain blocked. Confirmed blockers:

- Generic inference only unifies function parameters, not results. A helper
  such as `fn new<T>(args: opaque): ?*T` fails with `E3011 cannot infer
  generic argument; provide explicit type arguments inside an importing
  module`, and even an explicit `new<Box>(...)` call cannot force resolution
  once `opaque` packs and `dyn Allocator` are involved.
- Opaque pack values cannot be destructured/field-accessed in the current
  subset, so `args as |cap: u64|` compiles for matching but assigning
  `self.cap = raw tuple` fails with `E3001 expected 'i32', has type 'pack'`.
- The order-dependent global snapshot scans that made imported trait
  conformance unstable in populated workdirs are gone. Sema now resolves
  imported traits, type names, and owner methods through the current module's
  import bindings and namespace aliases; `tests/test-interface-satisfaction.cpp`
  covers populated-workdir qualified calls with `from std/memory` and several
  unrelated modules. Trait-default collection and dyn method lookup still scan
  all loaded modules where the default/requirement was defined, so a future
  compatibility change to method-scope resolution can narrow those scans
  further.
- `@alignOf` only accepts structs in the current subset, so heap
  `new<T>`/`make<T>` cannot query alignment for primitive-layout `T` without
  compiler work.

`destroy` remains object-owned for the legacy I/O types. The allocator owns
only raw storage. `HeapAllocator` uses C `malloc`/`free`/`realloc` and ignores
alignment in the heap path.
