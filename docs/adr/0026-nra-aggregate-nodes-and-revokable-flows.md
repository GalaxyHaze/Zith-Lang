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
compiler inserts an internal revocable proxy at the unbounded boundary. The
proxy uses a control block separate from the resource, with an atomic pointer
and atomic lease state. Direct proxy operations may return `Nil`; `acquire()`
returns a scoped, qualifier-preserving guard with normal access and no repeated
revocation checks. The proxy is a runtime detail with no source type name, as
the Source Spelling section below states. The core keyword `revoke` accepts
either a revocable resource or a thread handle. `revoke x;` closes new
acquisitions for `x` from every unbounded thread in the statement's scope that
holds revocable access to it, then waits for their active guarded operations to
finish. It does not terminate those workers. `revoke h1;` applies this
transition to every revocable resource passed to the thread represented by
handle `h1`. Revocation does not destroy resources or consume thread handles;
`merge` remains responsible for joining and consuming a handle.

Automatic allocator migration is intentionally not part of ordinary unbounded
transfer. The preferred pattern is flow-local construction: capture trivial
configuration, provide a child allocator, and construct the resource in the
child. `Transferable` remains an explicit advanced capability rather than an
implicit deep-copy or allocator-rewrite mechanism.

## Source Spelling

Status: accepted, then superseded in scope by
[ADR-0034](0034-nra-surface-spellings.md). This section introduced the
revocable source surface. ADR-0034 keeps the spelling rule but widens the
prefix to every qualifier except a bare `T`, so the body below is kept for
history and the current rule lives in ADR-0034 and
[nra-spec.md](../nra-spec.md) section 4.

The revocable contract is a type qualifier, not a wrapper type. It is written
as a sigil `'` before an ownership qualifier, with `grant` as the
long spelling.

| Spelling | Meaning |
|---|---|
| `own T` | owned, not revocable |
| `'own T` | revocable own |

ADR-0026 originally restricted the sigil to `own`, so `'lend T` and `'view T`
did not exist. ADR-0034 supersedes that restriction: the prefix now combines
with every qualifier except a bare `T`, so `'lend T` (revocable `&mut T`) and
`'view T` (revocable `&T`) also exist. A bare `'T` still does not exist,
because a revocable `default` would imply a logical move of an inline value,
while the contract is a reference whose access can be revoked.

`grant` is the long spelling of the same prefix, so `grant own T` equals
`'own T`, and likewise for the other qualifiers.

The sigil applies only in type position. A call argument does not repeat it.
The `unbounded` launch site already marks where revocation happens, so the
argument carries no revocable annotation. The call-argument parser keeps its
existing `lend` / `view` surface unchanged.

Lexical rule: after an opening `'`, one character or one escape followed by a
closing `'` is a character literal. Any other body starts a revocable type
qualifier. This is the rule Rust uses to separate the `'a` lifetime from the
`'a'` character. The scanner must tighten character literals to exactly one
character or one escape for this rule to hold, because the current scanner
accepts a multi-character body.

`grant` and `revoke` are the verb pair. `grant` declares the contract on a
type, and `revoke` ends it at runtime.
