# MRA Spec - Memory Region Analysis (Draft)

This document is a future-design spec for Memory Region Analysis (MRA). MRA
treats static hardware/virtual memory regions as first-class, array-like
regions with permissions and optional typed streams.

MRA is one of the four `Zith Proof Kernel` (ZPK) sub-systems. It defines and
controls memory regions. It does not prove ownership, lifetime, or borrow
correctness.

## 1. Scope

Safe Zith code does not permit arbitrary addresses. In `raw`/`unsafe`, an
address is only usable when it is contained in a declared memory region and
the requested operation is permitted.

MRA is not an ownership analysis and not a borrow checker. It provides region
shape, permissions, stream typing, and allocator region identity. RRA verifies
the geometry and NRA verifies the surrounding ownership context.

## 2. Region Kinds

MRA defines three static declarations:

| Kind | Purpose | `size` |
|---|---|---|
| `region` | Contiguous memory with a static shape. Hardware, arena, scratch. | constant or symbolic |
| `heap` | Dynamic allocation domain. The compiler knows provenance, not total size. | `dynamic` |
| `pool` | Fixed-count typed region. Slots are `0..count`. | derived from `count * sizeof(T)` |

All three are static declarations known to the compiler. There is no runtime
region table and no runtime lookup feature.

## 3. Region Declaration

```text
region Scratch:
    base: unknown
    size: 64 KiB
    access: read write
    init: true
    stream: untyped
    layout: bump
    alignment: 16

heap OsHeap:
    base: unknown
    size: dynamic
    access: read write
    alignment: 16

pool NodePool(Node, 64):
    base: unknown
    access: read write init
```

`base` is either a constant address or `unknown`. `unknown` means the compiler
will not know the final address at static analysis time. The region identity
and shape remain static, while the actual address is resolved by `init`.

`size` is constant/symbolic for `region` and `dynamic` for `heap`. A `pool`
derives its full shape from `pool P(T, N)`:

```text
size      = N * @sizeOf(T)
stride    = @sizeOf(T)
alignment = @alignOf(T)
slot(k)   = base + k * stride
```

The declaration should not repeat derived fields. For a dynamic heap, the
useful size appears on each allocated block, not on the heap declaration.

| Field | Meaning |
|---|---|
| `base` | Start address, constant or `unknown`. |
| `size` | Byte length. `dynamic` for heaps; derived for pools. |
| `access` | `read` and/or `write`. `write` implies read on the same access. |
| `init` | The region may be initialized once from `Uninit`. |
| `stream` | `untyped` bytes or `typed T` access. |
| `alignment` | Minimum alignment; derived for pools. |

## 4. Region State

`init` is a state transition, not a persistent permission:

```text
Uninit --init--> Init
```

| State | Permitted |
|---|---|
| `Uninit` | `init` only. |
| `Init` | `read` and/or `write` according to the region declaration. |

`init` is single-site for `base: unknown`. Only the initialization site may
resolve the runtime address. For constant-base regions, `init` may be a
program-start initialization event, not a permit to reinitialize.

```zith
fn initScratch() {
    @regionInit(Scratch, @mmap(64 KiB, ...), 64 KiB);
}
```

`@regionInit` resolves the runtime base once and records the domain size so RRA
can prove a block is contained by the heap/region. Allocator policy such as
bump, free-list, or bitset is not MRA state. It lives in the allocator
implementation.

Until the state model is formalized, overlapping regions with different
initialization states are rejected conservatively.

## 5. Typed And Untyped Streams

All region accesses use `@regionAt`. A typed region uses its declared type; an
untyped region is a byte stream:

```text
let p = @regionAt(VGA, 0x0400000C..0x04000CFF): *char;
```

A typed region is accessed through a declared element type:

```text
region Framebuffer:
    base: 0x04000C00
    size: 0x1000
    access: read write
    stream: typed Pixel
```

Typed access advances in `sizeof(T)` steps and must be aligned/proven within
the region. A `pool NodePool(Node, 64)` gives the compiler a static slot index
`k`, so RRA can prove `disjoint(k1, k2)` when NIA proves `k1 != k2`. For
untyped regions, `@regionAt(region, offset)` behaves like a byte-stream access
at `base + offset`.

### Ptr, Block, And Slice Results

`@regionAt` produces a singleton `Ptr<R>` when it targets one element, and a
`Block<R>` or slice when it targets an interval:

```text
let one = @regionAt(Framebuffer, 12): Ptr<Framebuffer>;   // singleton
let row = @regionAt(Framebuffer, 0..64): Block<Framebuffer>; // interval
```

A `Ptr<R>` is useful as identity for ownership checks; a `Block<R>`/slice is
useful when the proof needs bounds and disjointness.

## 6. Access Intrinsics

The initial MRA surface is:

| Intrinsic | Meaning |
|---|---|
| `@readRegion(region, T, range)` | Read `range` as `T` from a static region. |
| `@writeRegion(region, T, range)` | Write `range` as `T` to a static region. |
| `@regionOf(expr)` | Return the static region name that contains a constant/proven address. |
| `@regionContainsRegion(range, region)` | Return whether one range is contained in a region. |
| `@regionBounds(region)` | Return `{base, size}` for a static region. |
| `@regionPermissions(region)` | Return `{read, write, init}` for a static region. |
| `@regionAt(region, offset)` | Build a `Ptr<R>` or `Block<R>`/slice at `base + offset` from the declared stream; requires containment and permission. |
| `@regionInit(region, addr, size)` | Resolve `base: unknown` and record the domain size at a single initialization site. |
| `@regionIndex(region, ptr)` | Return the slot index for a pool pointer. |
| `@poolSlot(pool, index)` | Build a typed slot pointer at `base + index * stride`; validates the index and permission. |

These intrinsics resolve during compilation. A runtime lookup is intentionally
not provided because every region is static.

Example:

```zith
#writeRegion(VGA, *char, 0x0400000C..0x04000CFF);

let name = @regionOf(0x04000010);
let ok = @regionContainsRegion(0x0400000C..0x040000C4, VGA);
let base = @regionBounds(VGA).base;
let canWrite = @regionPermissions(VGA).write;
```

## 7. Ambiguity

If an address can belong to more than one declared region, `@regionOf` is a
compile-time ambiguity diagnostic. The programmer must make the region
explicit.

If RRA cannot prove that a dynamic access fits a single region, the access is
rejected unless it is explicitly marked `raw` with an opt-out that MRA and
NRA still understand.

### Dynamic Region Overlap

A dynamic `heap` has `base: unknown`; if it is initialized at runtime, MRA
cannot statically know its exact address unless `@regionInit` receives a
constant/proven address. Two dynamic regions may be backed by overlapping
physical memory, which breaks RRA disjointness across heaps.

This remains an open problem except for the conservative rule already present:
while the spec is immature, overlapping regions with different initialization
states are rejected. For dynamic heaps, the following strategies are candidates:

- Require the programmer to declare disjoint domain namespaces and treat each
  `heap` as an opaque provenance domain, even when physical overlaps exist.
- Require `@regionInit` results to be RRA-provable disjoint after initialization.
- Reject two `heap` values whose `@regionInit` calls cannot be proven disjoint.

The overlap rule will be resolved before the MRA implementation starts.

## 8. Allocators Over MRA

`Allocator` is a capability in Zith, parameterized by a comptime region. A
heap can be dynamic while keeping a static proof identity:

### Contract Intrinsics

ZPK gives allocator writers three explicit contracts:

```text
@assume(cond)   # installs cond as a trusted premise; contradiction is an error
@ensure(cond)   # caller-side contract: caller must satisfy cond before the call
@maybe(cond)    # requires the solver state for cond to be exactly Maybe
```

They do not turn MRA into an allocator verifier. The intrinsic is dispatched to
the sub-system that can prove the condition:

| Condition | Dispatched to |
|---|---|
| Numeric bounds, strides, ranges | NIA |
| Geometry, overlap/containment | RRA |
| Region/block/stream permission and shape | MRA |
| Lifetime, ownership, escape, free pairing | NRA |

An allocator may declare its own rules through `@ensure`:

```zith
implement BumpAllocator as Allocator(Scratch) {
    @ensure(size >= 0);
    @ensure(offset + size <= @regionBounds(Scratch).size);
    ...
}
```

Callers of `BumpAllocator.alloc` must satisfy these premises before the call;
the compiler may still prove or verify them with NIA/RRA instead of trusting
the allocator blindly.

```zith
struct Block<R> {
    ptr: Ptr<R>,
    len: u64,
}

capability Allocator(R):
    fn alloc(self, size: u64, align: u64): Block<R>!
    fn free(self, block: Block<R>): unit!
    fn realloc(self, old: Block<R>, size: u64, align: u64): Block<R>!

implement BumpAllocator as Allocator(Scratch) {
    fn alloc(self, size: u64, align: u64): Block<Scratch>! {
        let offset = @alignUp(self.cursor, align);
        if (offset + size > @regionBounds(Scratch).size) {
            return fail OutOfMemory {};
        }
        self.cursor = offset + size;
        return Block<Scratch> {
            ptr: @regionAt(Scratch, offset),
            len: size,
        };
    }

    fn free(self, block: Block<Scratch>): unit! {
        // Bump allocators reclaim on reset; individual free is a no-op.
    }

    fn realloc(self, old: Block<Scratch>, size: u64, align: u64): Block<Scratch>! {
        return fail NotSupported {};
    }
}

implement HeapAllocator as Allocator(OsHeap) {
    fn alloc(self, size: u64, align: u64): Block<OsHeap>! {
        if (self.base is null) {
            return fail HeapUninitialized {};
        }
        let mem = malloc(size);
        if (mem is null) {
            return fail OutOfMemory {};
        }
        return Block<OsHeap> { ptr: mem as Ptr<OsHeap>, len: size };
    }

    fn free(self, block: Block<OsHeap>): unit! {
        free(block.ptr as raw opaque);
    }

    fn realloc(...): Block<OsHeap>! {
        return fail NotSupported {};
    }
}
```

`R` is a comptime region parameter, not a runtime value. `Block<R>` carries
provenance and length to RRA/NRA without a runtime region table. Allocation
failure is a Zith `!` result, not an optional `?T`; `?T` belongs to `Zith--`
and is not part of the full Zith contract.

The MRA boundary is proof metadata, not executable allocator state. The
allocator itself may keep a runtime cursor, bitset, free-list, or heap
metadata. MRA records only what was proven about the region. It does not move
blocks between `Used`/`Free`.

### Pool Allocator

A `pool` does not need an explicit `size` field in its declaration. Its shape
is derived from `P(T, N)`, and a pool allocator returns an index:

```zith
pool NodePool(Node, 64):
    base: unknown
    access: read write init

struct PoolAllocator {
    used: u64, // runtime bitset; not MRA state
}

implement PoolAllocator as Allocator(NodePool) {
    fn alloc(self, size: u64, align: u64): Block<NodePool>! {
        if (size != @sizeOf(Node) || align != @alignOf(Node)) {
            return fail WrongSize {};
        }
        let index = @firstFree(self.used)!;
        self.used = @setBit(self.used, index, 1);
        return Block<NodePool> {
            ptr: @poolSlot(NodePool, index),
            len: @sizeOf(Node),
        };
    }

    fn free(self, block: Block<NodePool>): unit! {
        let index = @regionIndex(NodePool, block.ptr);
        self.used = @setBit(self.used, index, 0);
    }

    fn realloc(self, old: Block<NodePool>, size: u64, align: u64): Block<NodePool>! {
        return fail NotSupported {};
    }
}
```

`@poolSlot(NodePool, index)` is the idiom for pool access. RRA can prove
`Disjoint(slot(k1), slot(k2))` when NIA proves `k1 != k2`.

Bump allocators keep `free` in the capability contract, but the implementation
is a no-op: memory is reclaimed by a region reset, not by individual blocks.
NRA must therefore not treat a no-op `free` as a proof that a block is dead; a
bump block remains valid until the region resets or its scope ends.

### Heap Blocks

A dynamic heap returns `Block<OsHeap>`; NIA proves `0 <= lo`, `lo <= hi`, and
`hi <= block.len`; RRA proves slices are contained by the block; NRA proves the
block is not used after `free` or by a different allocator.

### Dynamic Domains And Guard Lines

Two dynamic heaps may physically overlap because `@regionInit` receives
runtime addresses. At the domain level, MRA conservatively marks them
`Overlap` unless `@regionInit` proves otherwise. This does not decide access
legality: it only records that the heap domains are not proven disjoint.

For the initial model, a guard-line permits at most one dynamic domain active
at a time during the proof. While `HeapA` is active for a call or region scope,
accesses from another dynamic heap are rejected unless the access itself is
proven disjoint. This avoids having to prove global heap non-overlap before
every allocation.

MRA does not implement the guard-line. NRA enforces the active-domain policy
and RRA proves per-access geometry.

### Facts By Kind

```text
region Scratch:
  shape: { base, size, alignment }
  permission: access
  access_range -> contains(Scratch, offset, len)
  block -> Ptr<Scratch>

heap OsHeap:
  shape: { base: unknown }
  permission: access
  provenance -> Ptr<OsHeap>
  block -> Block<OsHeap> { ptr, len }

pool NodePool:
  shape: { count: 64, elem: Node, stride: @sizeOf(Node) }
  slot(k) -> base + k * stride
  disjoint(k1, k2) when NIA proves k1 != k2

dynamic domains:
  domain(H1) and domain(H2) => Overlap unless init proves Disjoint
  active(H1) => H2 accesses rejected unless access-level Disjoint
```

## 9. Open Questions

- Exact declaration syntax for regions.
- Whether overlapping regions are allowed and how precedence is reported.
- Cursor semantics for typed streams.
- Runtime-safe helper functions over static regions.
- Whether `init` can be performed by more than one caller or only by one
  initialization site.
- Whether `layout` is declared in MRA or remains entirely an allocator detail.
