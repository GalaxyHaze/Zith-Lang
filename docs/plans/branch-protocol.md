# Branch Protocol Design

## Status

Draft design for the full-Zith thread protocol: `fork` as a core keyword,
`merge` as a core keyword, backend handles such as `pThread`, and `spawn` as a
stdlib shorthand activated by a context. Implementation is not started.

> This is full-Zith planning, not a Zith-- deliverable. Zith-- keeps explicit
> thread/runtime APIs out of core syntax until the runtime/stdlib surface and
> full ownership proof are defined. See `docs/roadmap.md` F-18/F-19/F-20 and the
> archived `docs/plans/archive/0.7.0-zith/` material.

## Goal

Zith keeps `state` machines as the deterministic control-flow mechanism and
explicit threads as the only concurrent execution boundary. The branch
protocol defines how a user action can be handed to a thread backend and how a
`share` value may participate in that transition.

The protocol deliberately avoids `async`, `await`, futures, and an implicit
runtime scheduler. `merge` is blocking, one-shot, and consumes the handle.

## Terminology

| Term | Meaning |
|---|---|
| `Fork` | The action contract implemented by a user entry point. |
| `Branch` | The conceptual execution boundary created by `fork`; not a separate runtime type. |
| `fork` | Core keyword: `backend fork Entry(args)` creates a thread through the backend object. |
| `merge` | Core keyword: waits for the thread, consumes the handle once, and returns the entry result. |
| `spawn` | Stdlib shorthand for the implicit fork: `spawn Entry(args)` uses the active backend context. |
| `Thread<T>` | Minimum owned, single-consumer handle type accepted by `merge`. |
| `ThreadBackend` | Capability/interface implemented by runtime objects such as `pThread`. |
| `forkCount` | Ownership counter tracked by NRA for a `share` node used by a fork. |

## Capability Definition

The user entry point is a `Fork` implementation. The backend object is a
`ThreadBackend` implementation that returns a concrete handle type.

```zith
capability Fork {
    // The entry action. The body normally delegates to a ThreadBackend.
}

capability ThreadBackend {
    type Result<T>;   // concrete handle type, e.g. PThreadHandle<T>
}
```

The compiler knows `fork`, `merge`, and the handle contract by keyword and
capability name for ownership purposes. Concrete backend types expose extra
methods beyond the `Thread<T>` minimum.

## Syntax

```zith
let t: PThreadHandle<i32> = pThread fork Update(share state, n);
let v: i32 = merge t;   // entry signature says Update returns i32
```

Initial design:

- `backend fork Entry(args)` requires `backend` to be a value implementing
  `ThreadBackend` and `Entry` to implement `Fork`.
- `merge t` requires `t` to implement the `Thread<T>` handle contract and
  returns exactly the `T` declared by the entry point.
- The returned handle is the concrete backend handle, not a language-owned
  generic `Thread<T>` box.
- `spawn Entry(args)` is a stdlib shorthand that resolves the active
  `ThreadBackend` from a context; it lowers to the same fork semantics.

Entry results use the normal Zith failable types. A thread creates and waits
without speaking about whether the action produced an error. If the action can
fail, its signature is `...: T!` and `merge t` yields `T!`.

## Example

```zith
use threading.pthread;   // activates spawn; pThread is in scope

state Update(n: i32): i32 { return n * 2; }

let t = pThread fork Update(3);
let result = merge t;

let shorthand = spawn Update(3);   // uses active backend
let also = merge shorthand;
```

## `detach` And Live Handles

`fork` does not change the `forkCount` policy for `share` values. A live handle
at scope exit is an ownership error. A backend may provide `detach(self)` as a
method that consumes the handle. It still counts as an unmerged fork because the
result and shared resource remain with the detached branch.

- Detached threads are opt-in through the concrete backend handle.
- Detaching is not a core keyword and does not reset `forkCount`.
- The compiler emits `branch still live` for a handle that reaches scope exit
  without `merge` or an explicit backend-owned consume.

## NRA Contract

Every `share` node that is passed to `fork` carries a `forkCount`:

```text
0: the node is still owned/controlled by the source.
1: the node is handed to one branch and is pending merge.
>1: invalid for this model; a second fork while one is live is rejected.
```

Transition rules:

1. `fork` increments `forkCount` and creates the backend handle.
2. While `forkCount != 0`, the source may not be moved, reassigned, or forked
   again.
3. `merge t` must consume the handle exactly once.
4. `merge` decrements `forkCount` and returns the entry result; it does not
   reclaim a `share` value.
5. Ending a scope with `forkCount != 0` or with a live handle is an ownership
   error. Backend-owned methods such as `detach` still leave the fork pending.

The compiler emits diagnostics such as:

- `branch still live` for a handle that reaches scope exit unmerged;
- `fork source busy` for a second fork while one is pending;
- `handle abandoned` for a handle that is dropped without `merge`.

`Thread<T>` is an owned, single-consumer value. It is not `share`, `view`, or
`lend`; only `merge` consumes it at the language level, besides backend-owned
methods that document their own consume behavior.

## Runtime Surface

The stdlib ships runtime implementations, but they are ordinary `ThreadBackend`
objects, not parser magic:

```zith
struct PThreadHandle<T> { ... }

implement PThreadHandle<T> as Thread<T> {
    // runtime state: thread, result, join primitive
}

implement PThread as ThreadBackend {
    // pThread fork Entry(args): PThreadHandle<EntryResult>
}
```

Implementation notes:

- Backends may expose extra handle methods such as priority, affinity, cancel,
  or `detach`.
- No coroutine suspension point exists. `merge` blocks the current thread until
  the branch completes.
- No `await` syntax, no `Task<T>` scheduler, and no resumable future are added
  by this protocol.

## Comptime Role

`comptime` is not the engine of fork/merge. It may generate wrappers, merge
strategies, or friendly APIs over the protocol, but it cannot validate runtime
ownership because resources and scopes are outside the comptime value domain.

## Open Questions

- Whether `fork` initially accepts ordinary `fn`/`state` entries or only
  capability-typed entries.
- How `spawn` resolves the active backend when more than one context is enabled
  in the same scope.
- Whether standard backend handles should share a common concrete trait beyond
  `Thread<T>`.

## Related Roadmap Items

- F-20: NRA shared-resource facts for runtime concurrency APIs.
- F-18/F-19: runtime task/thread APIs.
- F-14: full NRA/ownership proof.
