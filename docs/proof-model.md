# Zith Proof Model (Draft)

Zith's future safety model is split into four cooperating modules. Each module
has a narrow responsibility so the pipeline remains testable and auditable.

| Module | Responsibility | Consumers |
|---|---|---|
| NIA | Numeric facts, ranges, versions, headers, control-flow joins. | RRA, MRA, HIR/codegen |
| RRA | Geometry of slices, arrays, and memory regions. | NRA, MRA, HIR/codegen |
| MRA | Static memory regions, permissions, typed/untyped streams. | RRA, NRA, codegen |
| NRA | Ownership, lifetime, and borrow decisions. | NIA, RRA, HIR, cache |

`NIA` is the numeric fact foundation. `RRA` is the geometry layer and the
bridge between ownership and memory regions. `MRA` makes hardware memory a
first-class, array-like entity. `NRA` remains ownership-focused and does not
reimplement numeric ranges or raw region permissions.

## 1. Pipeline

```text
Source
  -> Lex
  -> Scan
  -> Import
  -> Resolve
  -> Sema
  -> NIA
  -> RRA/MRA
  -> NRA
  -> HIR
  -> Codegen
  -> Cache
```

NIA runs after sema and before RRA so geometric decisions see numeric facts.
RRA consumes MRA region descriptors and publishes geometric facts for NRA.
NRA emits a stable HIR boundary with residual facts such as ownership,
`unchecked_bounds`, and region-validated raw access.

## 2. Facts At The HIR Boundary

| Kind | Example |
|---|---|
| NIA | `index < values.len`. |
| RRA | `Disjoint(values[1..4], values[5..8])`, `unchecked_bounds`. |
| MRA | `access range in VGA`, `write permitted`, `stream type`. |
| NRA | `lend`, `view`, `alive`, `dead`, `lent`, escape. |

Cache stores headers and residual facts so callers do not reanalyze callee
bodies.

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

## 4. Invariants

- Lazy proof is preferred: collect first, group, then resolve.
- Contradiction is not `Maybe`.
- `Unknown` keeps code conservative.
- No arbitrary addresses in safe code.
- No runtime region table; MRA regions are static.
- Recursion is avoided through function headers.
- Ownership and geometry remain separate so each analysis can be tested alone.
