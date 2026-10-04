# Branch Protocol Draft

Full-Zith concurrency design moved from a `Branch` capability with `fork`/`merge`
as method calls to a core `fork`/`merge` protocol with runtime backend objects.
This note is a durable summary of the current draft in
`docs/plans/branch-protocol.md`; Zith-- still keeps threads out of core syntax.

## Source Of Truth

The detailed design lives in `docs/plans/branch-protocol.md`. The spec chapters
`docs/10-concurrency.md`, `docs/04-traits-interfaces.md`, `docs/07-memory-model.md`,
and `docs/Zith-spec-full.md` reflect the same draft and should be edited together.

## Model

- `fork` is a core keyword: `pThread fork Update(share state, n)`.
- `pThread` is an ordinary backend object implementing `ThreadBackend`.
- `fork` returns the backend's concrete handle, not a compiler-owned handle.
- `merge t` blocks, consumes the handle once, and returns the entry result
  type. `merge h1 and h2` waits for both and returns a tuple; `merge h1 or h2`
  waits for both and returns a tagged union for the first to finish. Simultaneous
  completion is resolved in favor of the leftmost handle. The union covers all
  declared result types and need not distinguish handles with the same type.
- `revoke x;` removes access to `x` from every unbounded thread in the
  statement's scope that holds revocable access to it. `revoke h1;` revokes all
  revocable resources passed to the thread represented by handle `h1`. Both
  forms prevent new guarded accesses and wait for active guarded operations to
  finish. Neither form terminates a thread or consumes its handle.
- `spawn Entry(args)` is a stdlib shorthand for the active backend, activated
  through a context such as `use threading.pthread`.
- `Thread<T>` is the minimum owned handle contract; concrete backend handles may
  expose extra methods such as priority, affinity, cancel, or `detach`.
- `detach` consumes the handle but still counts as an unmerged `forkCount`.

## NRA

`forkCount` remains the core share-node rule: 0 is owned, 1 is pending merge,
and more than one pending fork on the same source is rejected. `merge` returns
the entry result and does not pretend to reclaim a `share` value. Live handles at
scope exit are ownership errors.
