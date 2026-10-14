<!--
Imported reference. Source: GalaxyHaze/Zith@5a4177fc:docs/adr/0039-full-zith-capability-and-purity-contracts.md (working tree, untracked).
Design reference only; not a decision of this repository. See ../README.md.
-->

# Full-Zith Capability and Purity Contracts

Status: accepted as a full-Zith design direction. The syntax is not implemented
and does not change Zith--.

Full Zith keeps capability-use requirements separate from purity and from the
derived NRA effect header. A capability such as `Fs` is a nominal trait that
names an ability, not a purity classification or a runtime effect mechanism.

`@using(X, Y)` is an open requirement: the reachable body must use each named
capability at least once, but it may use other capabilities. `@only(X, Y)` is
closed: the body may use only the listed capabilities. Its negative form,
`@only(not X)`, prohibits `X`; positive and negative entries are not mixed in
one list. A `@using` requirement on a block must be met by a reachable use
inside that block. Unreachable code does not satisfy a requirement. Unknown
capability usage violates `@only` and cannot satisfy `@using`. Cleanup actions
such as `defer` and `drop` participate in the analysis.

Capability requirements may describe function and callback contracts, appear
in structural contracts, and scope a block. A contract-level requirement
applies to the callable body that fulfills that contract, even when the clause
is not repeated on the method. How one contract-level clause is distributed
when it could apply to multiple callable requirements remains open. Callback
requirements participate in compatibility and call checking. Inferred
summaries and contract annotations are cached for cross-module validation.

Full Zith distinguishes three purity classifications:

| Annotation | Requirement |
|---|---|
| `@pure` | The function is deterministic and has no side effects. |
| `@valuePure` | The returned value depends only on explicit arguments. Side effects are permitted when they do not affect that value. |
| `@impure` | The function is classified as impure. |

An unannotated function is analyzed and classified from its reachable body. It
is not presumed impure, and a context that requires purity accepts it only when
the inferred classification satisfies that requirement. Inference is
transitive through calls. The compiler classifies each reachable return using
the conditions and operations on its path, then combines those classifications
for the function. A return value that depends on an impure operation makes the
function impure, even if that operation's side effect is otherwise
unobservable. For example, returning whether `Allocator.alloc` succeeded is
impure because the result depends on the allocation outcome.

Purity and capability-use requirements are independent dimensions, but a
combination can be contradictory. For example, `@pure` cannot be combined
with `@using(Fs)` when using `Fs` is impure. Purity and capability obligations
are checked statically. The compiler does not insert runtime checks. Extern
and raw declarations supply trusted contracts rather than body proofs.
Recursive call groups are classified to a least fixed point, so a recursive
group remains pure when it has no impure operation. A dedicated compile-time
diagnostic family reports violations; its numeric code range is not assigned
here. The position and ordering of annotations, including `@pure` on callback
types, remain open. The compatibility ordering between the three annotated
callable types also remains open.

This decision does not redefine the `pure` fact in NIA or the effect vocabulary
in the NRA header. It adds full-Zith source contracts over capability use and
function result behavior. Zith-- remains unchanged.
