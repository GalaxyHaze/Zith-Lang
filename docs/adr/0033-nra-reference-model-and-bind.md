# NRA Reference Model: `&`, `^` Bind, and `%` Own

Status: accepted as a full-Zith design direction. Nothing in this decision is
implemented or changes Zith--.

> Spellings completed by [ADR-0034](0034-nra-surface-spellings.md): `own` is
> the keyword for `%mut T` and `bind` the keyword for `^mut T`. Keyword and
> sigil coexist and are equivalent, so the `own`/`bind` examples below are
> current. `belong` is removed.

## Context

The earlier NRA design had separate `lend`, `view`, `share`, and `belong`
categories, with `lend` exclusive and `view` an anchor. Keeping them apart made
the model harder to learn and left `belong` unclear next to `view`.

## Decision

`&T` is a read reference and `&mut T` a write reference. `lend`, `view`, and
`share` are absorbed into them. `^T` is a bind and `%T` is an own. The
mutable forms are the common ones. References may coexist in one flow.
Exclusivity is checked at a boundary (call, return, flow crossing, closure
capture): many readers or one writer. `MultiWrite` is the explicit exception
for several writer flows.

The NRA has two edges, owning and non-owning. A non-owning edge is a lifetime
dependency, `lifetime(B) <= lifetime(A)`, checked lazily at use. `&` is that
edge with a region limit and a pinning veto. `^` is the same edge with no
region limit and no pinning. A bind is a general lifetime edge, not only a
part-of edge, and it can be a parameter or a return value.

- `&` cannot be stored in a field, global, or durable capture. It can be passed
  and returned. A returned `&` of a parameter takes the caller's region.
- `&` pins its source against physical relocation and consumption, per subgraph.
  A borrow of `s.a` does not conflict with sibling `s.b`.
- `^` is invalidated when its parent is consumed or relocated. It does not pin.
  `let b = a` is a retarget and invalidates nothing.
- Invalidation is reported at the next use, with the cause taken from the
  provenance list. `&` words it as a source that cannot be moved, `^` as a
  resource that became invalid.
- `&` and `^` never convert to `*`. Only `T` and `own` can create pointers. The
  `c/` contract maps C functions to NRA effects (`read`, `write`, `borrow`,
  `move`, `retain`).
- Lifetime cycles are rejected over the combined own and bind graph. Cycle
  detection uses SCC or DFS, not union-find. Union-find only groups nodes, one
  disjoint-set per equivalence relation.

## Considered Options

`^` was first judged possibly redundant next to `&`. It is kept because it is
the base edge: `&` is `^` plus a region limit and a pin. Making `^` also pin
was rejected, so that a bind stays cheap and its failure is a lazy invalidation.

## Consequences

A bind assumes a stable target address, so durable structures such as a doubly
linked list allocate their nodes: `next: Option<own Self>` and
`prev: Option<bind Self>`. Containers may later offer stable and reallocating
method families.
