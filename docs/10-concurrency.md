## 10. Concurrency & Runtime APIs

> **Implementation status:** concurrency is not a core syntax feature. The compiler does not model
> `async fn`, `yield`, `spawn`, or `await` as language constructs. Any future concurrency support is
> expected to arrive through `stdlib` and runtime APIs built from ordinary functions, types, and
> NRA-checked resource rules. See [impl-status.md](impl-status.md).

> **Full-Zith draft:** the branch protocol below is a design draft, not implemented syntax. It is
> tracked in [docs/plans/branch-protocol.md](plans/branch-protocol.md).

### 10.1 Core-Language Position

Zith's core language defines `fork`/`merge` as explicit thread statements, but
does not define `async`, coroutines, schedulers, or a concurrency function kind.
There are no dedicated HIR nodes for `await` or coroutine suspension. The
compiler understands only:

- ordinary declarations and calls
- the `fork` and `merge` thread protocol
- library-defined handle, channel, task, or executor types
- traits/capabilities used to describe what those types guarantee
- NRA facts about sharing, lending, capture, escape, and ownership across those
  calls.

### 10.2 Runtime Surface

The standard library or an alternate runtime may expose APIs such as thread
spawners, executors, message queues, join handles, or resumable tasks. `spawn`
is a stdlib shorthand for an implicit fork and is activated through a context.
the core protocol itself is explicit:

```zith
use threading.pthread;

let handle = pThread fork Worker(share state);
let result = merge handle;

let shorthand = spawn Worker(share state);   // active backend
let out = merge shorthand;
```

API names above are illustrative. Scheduling helpers are not reserved. `fork`,
`merge`, and the `Thread<T>` handle contract are the stable language surface.

### 10.3 Thread Fork/Merge

The explicit thread protocol uses `fork`/`merge` as core keywords, with runtime
backends as ordinary objects. There are no coroutines, `await`, or implicit
schedulers in the core language:

```zith
let t: PThreadHandle<i32> = pThread fork Update(share state, n);
let result: i32 = merge t;
```

`fork` hands an entry action to a backend object and returns the backend's
concrete handle (`Thread<T>` minimum). `merge` blocks, consumes the handle once,
and returns exactly the result type declared by the entry. `spawn Entry(args)`
is a stdlib shorthand that uses the active thread backend. It is not a core
keyword. NRA tracks the fork as an ownership transition and rejects unbalanced
forks. See [the branch protocol plan](plans/branch-protocol.md).

### 10.4 What the Compiler Proves

Concurrency-related safety is enforced through the same pre-HIR ownership proof used everywhere
else:

- whether a call duplicates a resource illegally
- whether a borrowed or `belong` value escapes
- whether narrowing facts or branch facts justify later lowering decisions
- whether shared/runtime-managed resources are passed only through the capabilities and wrapper types
  that define the contract.

The compiler does not special-case threads or async control flow. If a runtime API needs stronger
guarantees, it must express them through normal signatures, types, and traits that NRA can reason
about before HIR is finalized.

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
