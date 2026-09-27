# NRA Future Design

Short-lived operational pointer for the future Zith NRA contract. The full spec
is `docs/nra-spec.md`; the current `Zith--` implementation status is in
`docs/impl-status.md` and must not be mixed with this design.

## Decisions

### Resource graph

- The core resource graph is hierarchical. An `AggregateNode` represents a
  complete value and contains `FieldNode` children only for ownership-relevant
  fields. Plain scalar fields do not need independent nodes.
- Resource identity is independent of bindings. A binding is a name pointing to
  an `AggregateNode` or `FieldNode`; moving a name does not recreate the
  resource.
- Aggregate moves require every ownership-relevant child to be available.
  `liveFieldCount` and `requiredFieldCount` are fast checks, not replacements
  for per-field state or diagnostic provenance.
- A partial field move consumes only that field. Sibling fields stay alive, but
  a whole-aggregate move is rejected until every required field is available.
- Returned values and fields carry node-level provenance. `returnsField` can
  identify the exact source node rather than only an argument index.

### Move and assignment semantics

- `let b = a` normally retargets the binding `b` to the resource identity
  referenced by `a`; it is not automatically a physical copy.
- A physical move is used between struct fields or compatible value slots.
  The source field becomes uninitialized and can be restored with `=`.
- A logical move consumes a storage identity, address, or ownership edge.
  `T -> own T` and `own T -> own T` are logical moves. `T -> T` and
  `own T -> T` are physical moves.
- `=` writes/initializes a currently valid or physically moved slot.
  `:=` is reserved for reviving a logically moved aggregate binding by
  creating a completely new `AggregateNode`.

### Views and flows

- `view` is lazy by default. Invalidation does not fail immediately; a later
  access to the invalidated view reports the error. A view remains associated
  with its original node ID, not with a binding resolved again later.
- A view is an alias to the same resource, not a snapshot. In-place mutation
  in the same flow remains visible and valid.
- Strict view policy is optional. It rejects conflicting moves/writes while
  anchors are active instead of deferring the error until use.
- Same-flow read/write coexistence is allowed according to ordinary NRA
  permissions. A fork creates a new execution flow and activates cross-flow
  restrictions.
- An aggregate with an active anchor cannot be transferred across flows in
  strict mode. In lazy mode the transfer can invalidate the anchor; a later
  view access then fails and the invalid anchor no longer contributes to
  lifetime.
- A field can cross a flow independently only when the transfer does not
  conflict with an anchor on that field or an ancestor aggregate. An
  aggregate-level anchor covers all relevant children.

### Thread modes

- `Thread.spawn(worker, optionally: waiter)` creates a blueprint and does not
  execute it. `.bounded(args...)` and `.unbounded(args...)` select the
  execution mode and start/configure the flow.
- A bounded flow is scoped by the parent and uses ordinary `view`/`lend`
  contracts. `merge`/`wait` restores or closes the relevant borrow state.
- An unbounded flow may outlive its parent. Borrowed parameters must explicitly
  declare a revocable contract; the compiler automatically inserts the
  `Revokable` wrapper at the unbounded boundary.
- An unbounded worker returns no value to the parent. It communicates through
  explicitly shared/revocable resources or runtime APIs.
- Discarding a handle is not an explicit detached operation. Compiler/runtime
  management keeps the flow protocol alive and auto-revokes parent-borrowed
  resources at scope exit.
- `h.revoke()` revokes the resources borrowed by that child but does not kill
  the thread. `h.wait()` waits for the worker; the two operations are separate.

### Revokable access

- `Revokable<T>` is a proxy/capability for `view T` or `lend T`. Direct proxy
  methods may return `Nil` when the resource has been revoked.
- `Revokable<view T>` supports multiple active readers. `Revokable<lend T>`
  and `Revokable<own T>` use one exclusive active writer.
- `acquire()` returns a `RevokableGuard<T>` that inherits the original
  qualifier. The guard validates access once, permits normal method dispatch
  without repeated revocation checks, and cannot escape its scope.
- The control block is separate from the resource. It contains the atomic
  resource pointer and atomic lease/revocation state and remains observable
  after the resource is cleaned.
- The lease state uses a numeric protocol: positive reader count, an exclusive
  writer state, and a revoking state that prevents new acquisitions. The
  pointer becomes null only after active guards are released.
- `Waiter` exposes only `wait()`. The thread implementation decides whether it
  waits for completion, merge, or revocation cleanup.
- Type erasure does not remove NRA effects. `Revokable<dyn Trait>` is a proxy
  around the erased object; it does not require the trait itself to implement
  revocation. A revocable proxy adds an outer optional result layer, so a
  method returning `?T` remains distinguishable from revocation.

### Transfer and allocation

- Automatic allocator migration while crossing a thread boundary is not the
  default. The recommended unbounded pattern is flow-local construction:
  capture trivial configuration, pass an allocator to the child, and construct
  the aggregate there.
- `Transferable` may exist as an explicit advanced capability, but it must not
  silently recreate allocator-owned fields in another memory domain.
- A bounded flow may rely on parent scope lifetime. An unbounded flow may not
  rely on parent-owned stack/allocator storage unless it uses the explicit
  revocable protocol.

- Function contracts remain derived and cacheable effect headers. Effects
  include `read`, `write`, `move`, and `borrow`; `capture`, `escape`, `alloc`,
  `free`, `fork`, and `merge` remain internal facts.
- `view` is an anchor, not an owner. It never destroys or promotes storage.
- `share` remains a separate static owner-group model. Its multi-writer policy
  is not folded into the `view`/`lend` revocation protocol.
- Cleanup order remains `fail -> defer -> drop -> storage free`.
- The current implementation status remains separate in `docs/impl-status.md`;
  this file records future NRA design decisions, not shipped Zith-- behavior.

## Rejected or deferred approaches

- Do not silently migrate strings, vectors, or other allocator-owned fields
  to a destination allocator during an unbounded transfer.
- Do not treat an atomic pointer alone as a lifetime guarantee. A pointer
  load must be protected by an active lease until the access ends.
- Do not make every worker implicitly revocable. The worker contract must
  explicitly declare revocable borrowing, even though the compiler inserts the
  wrapper at `.unbounded(...)`.
- Do not make a guard escape its lexical scope or collapse revocation `Nil`
  with a method's ordinary domain-level `Nil`.

## Open Areas

- `extern fn` effect-header attributes.
- Custom allocator details.
- Diagnostic catalog and examples.
- Exact cleanup ordering across every control-flow shape.
- `belong` field lifetime rules.
