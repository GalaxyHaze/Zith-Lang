# NRA Future Design

Short-lived operational pointer for the future Zith NRA contract. The full spec
is `docs/nra-spec.md`; the current `Zith--` implementation status is in
`docs/impl-status.md` and must not be mixed with this design.

## Decisions

- Core states: `alive`, `dead`, `lent`.
- Function contract: derived, cacheable effect-header.
- Effect-header fields: `read`, `write`, `move`, `borrow`; `capture`, `escape`,
  `alloc`, `free`, `fork`, `merge` are internal facts.
- Return provenance: argument index, new owned, borrow from argument, or view
  from argument.
- `view` is an anchor, not a weak owner; it delays cleanup but cannot destroy or
  promote stack to heap.
- `share` is a static owner group; the last static owner edge closes the
  resource.
- A join point keeps a binding `alive` only when every reaching path keeps it
  alive.
- Duplicate `view` arguments are allowed with a warning; mixed mutable access is
  an error.
- Cleanup order: `fail -> defer -> drop -> storage free`.
- `Allocator` is a capability. `unique`/`share` may use stack or heap.
- `MultiShare<T>` is a default capability/wrapper for fork only; forkCount and
  ownerThreadId exist only on promoted values.

## Open Areas

- `extern fn` effect-header attributes.
- Custom allocator details.
- Diagnostic catalog and examples.
- Exact cleanup ordering across every control-flow shape.
- `belong` field lifetime rules.

