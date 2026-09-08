# RRA Spec - Region Relationship Analysis (Draft)

This document is a future-design spec for Region Relationship Analysis (RRA).
RRA is the geometric layer between NIA, NRA, and MRA. It answers questions
about memory ranges and element regions; it does not update ownership state
and does not validate raw hardware permissions.

## 1. Scope

RRA proves how slices, arrays, and memory-region accesses relate to each
other:

```text
Same(a, b)
Contains(a, b)
ContainedBy(a, b)
Disjoint(a, b)
Overlaps(a, b)
Unknown(a, b)
```

For dynamic heaps, RRA distinguishes *domain* overlap from *access* overlap.
Two heap domains are `Overlap` unless `@regionInit` proves otherwise. A single
`Block<H>` or `Ptr<H>` access can still be proven `Disjoint` against another
access when NIA/RRA prove their concrete ranges do not touch.

RRA consumes numeric facts from NIA and shape facts from MRA, then publishes
geometric facts for NRA and HIR/codegen.

RRA is one of the four `Zith Proof Kernel` (ZPK) sub-systems.

## 2. Inputs

RRA receives:

| Source | Facts |
|---|---|
| Types | `base`, `len`, element type for arrays/slices. |
| NIA | Numeric bounds for indices, offsets, and lengths. |
| MRA | Static regions, heaps, and pools plus their address/shape facts. |

For a slice expression `array[lo..hi]`, RRA records:

```text
region = (base + lo, hi - lo)
```

When NIA proves `0 <= lo`, `lo <= hi`, and `hi <= len(array)`, RRA also
records that the slice is contained by the array.

## 3. Outputs

RRA outputs geometric decisions:

| Decision | Meaning |
|---|---|
| `Same` | Two regions denote the same element/byte interval. |
| `Contains` | One region fully contains another. |
| `Disjoint` | Two regions share no element/byte. |
| `Overlaps` | The regions intersect but neither contains the other. |
| `Unknown` | Current facts cannot decide. |

For code generation:

```text
unchecked_bounds := NIA proves 0 <= index && index < length
disjoint_access := RRA proves region(a) does not intersect region(b)
```

## 4. Ownership Boundary

RRA is not a borrow checker. It tells NRA whether two accessed regions touch;
NRA decides whether `lend`, `view`, or conflicting calls are legal.

Example:

```zith
let a = arr[1..4];
let b = arr[5..8];
lend a;
view b;
```

RRA proves `Disjoint(a, b)`. NRA uses that fact to allow the exclusive
temporary borrow and the read-only view in the same scope when the compiler's
other ownership rules are satisfied.

If RRA returns `Unknown` or `Overlaps`, NRA falls back to conservative
ownership handling.

## 5. Memory Region Boundary

RRA also acts as the bridge to MRA:

- MRA describes the static region/heap/pool (`VGA.base`, `Scratch.size`,
  `OsHeap` provenance, `NodePool` slot shape, permissions).
- NIA proves the requested access range.
- RRA proves the access is contained by the region.
- For `heap OsHeap`, RRA proves bounds against the returned block, not a
  static heap size.
- NRA supplies the ownership/escape context if the operation uses a pointer.
- Codegen emits the raw operation after the proof passes.

An unresolved or ambiguous region query is a diagnostic, not a runtime
fallback.

## 6. Bound Check Removal

The stable HIR may carry a residual `unchecked_bounds` decision on index and
slice nodes. This decision comes only from NIA/RRA facts, not from backend
assumptions.

```text
values[index]
----------------
NIA:  index >= 0
NIA:  index < values.len
RRA:  unchecked_bounds = true
```

The HIR counterpart already exists as `hir::HirMakeSlice::checked`; the future
model extends the same idea to index nodes and raw region accesses.

## 7. What RRA Does Not Do

| Not RRA | Owner |
|---|---|
| Numeric fact solving | NIA |
| Ownership/lifetime | NRA |
| Region permissions and stream typing | MRA |
| Runtime region lookup | none, regions are static |

## 8. Allocator Region Geometry

Allocator results carry a comptime region provenance such as `Ptr<Scratch>` or
`Ptr<OsHeap>`. RRA uses that provenance instead of treating raw storage as an
arbitrary address:

| Pointer | Geometry |
|---|---|
| `Ptr<Scratch>` | Range `base + offset .. base + offset + len` against `Scratch`. |
| `Block<OsHeap>` | Range `block.ptr .. block.ptr + len`; heap size is dynamic. |
| `Ptr<NodePool>` | Slot inequality gives `Disjoint(slot(k1), slot(k2))`. |

RRA does not decide whether a block is owned, still alive, or correctly freed.
Those are NRA decisions.

### Access-Level Disjointness

The guard-line allows one dynamic domain to be active at a time. Even when two
heap domains are not proven globally disjoint, individual accesses can be
compared:

```text
let a: Ptr<HeapA> = @regionAt(HeapA, 0..8);
let b: Ptr<HeapB> = @regionAt(HeapB, 100..108);

RRA:  Disjoint(a, b) if NIA proves the concrete ranges do not intersect
```

If the ranges cannot be proven disjoint and both heaps are active, NRA treats
the access conservatively. This keeps the theory honest without pretending
that dynamic heaps are globally disjoint by construction.
