# NRA Aggregate Nodes And Revokable Flows

Status: accepted

The future NRA design models resource identity as an `AggregateNode` containing
ownership-relevant `FieldNode`s. Physical moves between compatible fields leave
the source uninitialized and restorable with `=`, while logical moves consume
the source identity and require `:=` to create a new aggregate. This preserves
partial field moves without making bindings themselves the resource identity.

NRA treats same-flow access differently from cross-flow access. Lazy views do
not block same-flow mutation and report invalidation when the view is later
used; strict mode may reject the conflicting operation immediately. A field
may cross a flow independently only when no active anchor covers that field or
an ancestor aggregate.

Thread execution is explicitly selected from a `Thread.spawn` blueprint by
`.bounded(...)` or `.unbounded(...)`. Bounded flows use normal borrow
contracts. Unbounded workers must explicitly declare revocable borrowing; the
compiler inserts the `Revokable<T>` proxy at the unbounded boundary. The proxy
uses a control block separate from the resource, with an atomic pointer and
atomic lease state. Direct proxy operations may return `Nil`; `acquire()`
returns a scoped, qualifier-preserving guard with normal access and no repeated
revocation checks. `revoke()` closes new acquisitions but does not terminate
the worker, and `wait()` remains the only operation exposed by `Waiter`.

Automatic allocator migration is intentionally not part of ordinary unbounded
transfer. The preferred pattern is flow-local construction: capture trivial
configuration, provide a child allocator, and construct the resource in the
child. `Transferable` remains an explicit advanced capability rather than an
implicit deep-copy or allocator-rewrite mechanism.
