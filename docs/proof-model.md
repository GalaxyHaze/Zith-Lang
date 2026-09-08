# Zith Proof Kernel (ZPK) (Draft)

Zith's future proof and safety model is split into four cooperating
sub-systems. Together they form the Zith Proof Kernel (ZPK). Each sub-system
has a narrow responsibility so the pipeline remains testable and auditable.

| ZPK Sub-system | Responsibility | Consumers |
|---|---|---|
| NIA | Numeric facts, ranges, versions, headers, control-flow joins. | RRA, MRA, HIR/codegen |
| RRA | Geometry of slices, arrays, and memory regions. | NRA, MRA, HIR/codegen |
| MRA | Static memory regions, heaps, pools, permissions, typed/untyped streams, allocator regions. | RRA, NRA, codegen |
| NRA | Ownership, lifetime, and borrow decisions. | NIA, RRA, HIR, cache |

`NIA` is the numeric fact foundation. `RRA` is the geometry layer and the
bridge between ownership and memory regions. `MRA` makes hardware memory a
first-class, array-like entity and gives dynamic allocators a static proof
identity. `NRA` remains ownership-focused and does not reimplement numeric
ranges or raw region permissions.

## 1. Pipeline

```text
Source
  -> Lex
  -> Scan
  -> Import
  -> Resolve
  -> Sema
  -> ZPK
       NIA / RRA / MRA / NRA
  -> HIR
  -> Codegen
  -> Cache
```

`ZPK` is not a pass with a single implementation. It is the umbrella contract
for the four cooperating sub-systems. The order above is the dependency order:
NIA facts are ready before geometry, RRA validates ranges before ownership
nodes are finalized, MRA supplies region identity and permissions, and NRA
concludes ownership/lifetime.

NIA runs after sema and before RRA so geometric decisions see numeric facts.
RRA consumes MRA region descriptors and publishes geometric facts for NRA.
NRA emits a stable HIR boundary with residual facts such as ownership,
`unchecked_bounds`, and region-validated raw access.

When mentioned as a single named unit, "ZPK" refers to the coordinated
NIA/RRA/MRA/NRA set, not to a separate fifth analysis.

## 2. Facts At The HIR Boundary

| Kind | Example |
|---|---|
| NIA | `index < values.len`. |
| RRA | `Disjoint(values[1..4], values[5..8])`, `unchecked_bounds`. |
| MRA | `access range in VGA`, `write permitted`, `stream type`, `Ptr<HeapMain>` provenance. |
| NRA | `lend`, `view`, `alive`, `dead`, `lent`, escape. |

Cache stores headers and residual facts so callers do not reanalyze callee
bodies. At a call site each sub-system checks only the cached premises in its
own domain: NIA numeric facts, RRA geometry, MRA region/block/permission shape,
and NRA ownership/lifetime/escape.

## 3. Example

```text
let a = arr[1..4];
let b = arr[5..8];
lend a;
view b;
@writeRegion(VGA, *char, 0x0400000C..0x040000C4);
```

1. NIA proves the slice bounds for `a`, `b`, and the VGA access range.
2. RRA proves `a` and `b` are disjoint and the VGA range is contained.
3. NRA accepts the temporary borrow and view when ownership rules pass.
4. MRA/codegen emits the raw VGA write only after region permission checks.

For an allocator, the same pipeline works with a dynamic region:

```text
heap OsHeap:
  base: unknown
  size: dynamic
  access: read write

let block = allocator.alloc(...);
```

1. MRA records that the returned pointer is `Ptr<OsHeap>`.
2. RRA/NIA prove the requested range is valid for the block size.
3. NRA decides who owns the block and whether `free`/`release` matches its
   origin.
4. MRA/codegen emits the allocation operation without claiming the OS heap has
   a static size.

## 4. Invariants

- Lazy proof is preferred: collect first, group, then resolve.
- Contradiction is not `Maybe`.
- `Unknown` keeps code conservative.
- No arbitrary addresses in safe code.
- No runtime region table; MRA regions/heaps/pools are static declarations.
- MRA defines regions and permissions only; it does not prove ownership or
  lifetime.
- Allocator state is runtime/executable policy; MRA state is proof metadata.
- Recursion is avoided through function headers.
- Ownership and geometry remain separate so each analysis can be tested alone.

## 5. Naming

Suggested internal names for tooling and documentation:

- `ZPK-NIA` — Numeric Interval Analysis.
- `ZPK-RRA` — Region Relationship Analysis.
- `ZPK-MRA` — Memory Region Analysis.
- `ZPK-NRA` — Node Resource Analysis.

`NTA` is not used for the current future model. The interval/range work is
called `NIA`.
