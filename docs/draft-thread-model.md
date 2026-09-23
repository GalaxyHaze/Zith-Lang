# Zith Thread Model Draft

Status: discussion draft. This document records decisions made in the proof
model rounds and asks the remaining questions before it becomes a ZPK contract.

It is based on the upstream Zith thread protocol
(`docs/10-concurrency.md`, `docs/plans/branch-protocol.md`) and replaces the
current `forkCount` treatment only where explicitly stated below.

## 1. Upstream Shape Being Replaced

The upstream draft defines:

- `fork` / `merge` as core keywords.
- `spawn` as a stdlib shorthand for an implicit fork.
- `share` values crossing a thread boundary with a `forkCount`.
- `detach` as a backend-owned method that consumes a handle.
- No `async`, `await`, futures, or coroutines in the core language.

That model is not implemented yet and is still full-Zith planning. This draft
keeps the no-async position but changes the ownership surface around threads.

## 2. Decisions Made So Far

### 2.1 Scoped Threads Must Merge

Ordinary threads are scoped. A scoped fork is valid only when the handle is
merged before the scope exits. A live scoped handle at scope exit is an
ownership error.

There is no revocable wrapper for ordinary scoped threads. They use the normal
`view` / `lend` / ownership rules and end by `merge`.

### 2.2 Disjoint Threads Are Not Ordinary Fork/Merge

A disjoint thread is the detached case. It is the only case that receives a
revocable access wrapper over shared resources.

Revocation revokes the child thread's access to the resource. It does not
destroy the resource.

### 2.3 Revocation Uses An Atomic Wrapper

The wrapper is an atomic/synchronized object, tentatively described as
`RevocableAccess<T>`:

```text
RevocableAccess<T> :=
  Ptr<T}
  atomic state
```

The state purpose is:

- parent may revoke the child access;
- child operations are gated by the state;
- parent is blocked from revoking while a child operation is active;
- `promote()` enters a critical region where the per-operation check is not
  repeated.

Terminology decision: use `revoke` / `revoked`, not `invalidate` / `invalid`.
Revocation removes access, not resource identity.

Operation boundary decision: a child operation that started before revocation
completes with the state it captured. Revocation is only observable at an
operation boundary, never inside an in-flight operation.

Acknowledgement decision: `revoke()` requests revocation and waits for the
child to leave the active/promoted region before returning. Without that ack,
the parent cannot safely reuse the underlying storage.

### 2.8 Fork Syntax Draft

Tentative general form for a disjoint thread:

```text
fork[Backend] Entry(args) disjoint: Waiter;
```

Example:

```zith
fork[pThread] Worker(state) disjoint: Waiter;
```

In this draft, `Backend` names the thread backend, `Entry` is the child action,
and `Waiter` describes how the parent waits for acknowledgement or completion.
The exact meaning of `Waiter` is a capability: any type that implements
`Waiter` may be passed at that syntax position. Concrete backends provide their
own waiter implementation; the core language only defines the capability
contract.

### 2.4 Parent Binding While Thread Is Active

While a scoped child is active with a resource, the parent binding is not
freely usable. The two options under discussion are:

- the parent binding is `dead` / temporarily invalid for the thread lifetime;
- the resource is promoted to `MultiShare` for the thread lifetime.

Decision: the parent binding is temporarily `dead` for the thread lifetime.
The resource is not promoted to another type mid-scope. This keeps the state
explicit and avoids a hidden capability change.

### 2.5 Multiple Children And Access Combination

Multiple child threads may use the same resource when the access combination
is legal.

The legal combinations are:

- multiple `view` accessors;
- exactly one `lend` accessor;
- never `view` plus `lend` at the same time.

This is stricter than the current upstream `share`-only surface.

### 2.6 Child-Side Operations

On the child side, operations look normal but every operation may fail. The
exact failure shape is still open: optional result, result type, or panic is
not decided.

### 2.7 `lend` Across Threads

`lend` may cross a thread boundary in this model. It is not limited to the same
thread stack.

Thread crossing does not convert the shared resource into `share` or
`MultiShare`.

## 3. Proposed Vocabulary

Names below are tentative and should not be treated as ratified syntax.

| Term | Meaning |
|---|---|
| `scoped fork` | Ordinary thread bounded by `merge`. |
| `disjoint fork` | Detached-style thread with revocable access. |
| `RevocableAccess<T>` | Child-side wrapper that may be revoked by the parent. |
| `promote()` | Child-side operation that stops per-access checks inside a critical region. |

## 4. Next Round Questions

1. Syntax: should scoped threads stay as explicit `fork` / `merge` pairs, or
   become a lexical block such as `scoped { ... }` with implicit joins?
2. Syntax: is `disjoint` a fork modifier, a backend handle method, or a
   separate core keyword?
3. Lifetime: can a disjoint child access a parent stack resource if the parent
   revokes and waits for acknowledgement before the stack frame ends, or must
   disjoint resources be owned/heap-backed?
4. Parent state: for a scoped child, is the parent binding `dead`, `lent`, or
   promoted to `MultiShare`?

   Decided: the parent binding is temporarily `dead`, not promoted.
5. `Waiter`: what is the semantic contract of the type after `disjoint:`?
   Is it a wait strategy, a capability, a result carrier, or a backend-provided
   handle?

   Decided: `Waiter` is a capability. Any type implementing it may be supplied.
   Backends provide concrete implementations; the core language does not
   dictate a single concrete waiter type.
6. Share: is `share` still needed after `view` / `lend` / `RevocableAccess`
   cover the scoped and disjoint cases?
7. Revocation: what happens to a child operation that begins after revocation?
   Does it return an optional result, fail, or become a compile-time-only rule?
8. Backend: does `ThreadBackend` remain a trust boundary? The draft assumes it
   still needs explicit effect headers for `fork`, `merge`, and `disjoint`.

The minimum set to close next is 1, 3, 4, 5, and 7.
