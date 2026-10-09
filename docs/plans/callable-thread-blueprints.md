# Callable Thread Blueprints

Status: discussion draft. This records the thread and callable direction
discussed on 2026-10-06. It does not change Zith-- or replace an accepted ADR.

This draft separates a reusable job description from a launched process handle.
It also records the open relationship between function values, closures, and
`Functor`, which affects how a job accepts its work value.

## Current Direction

### Job blueprint

`Thread` is the default standard-library thread backend. Other backends remain
explicit values, such as `pThread`.

`Thread.job(work)` receives only the work value. It creates a blueprint and
does not launch anything. Invocation arguments are supplied when the blueprint
starts:

```zith
let job = Thread.job(Worker);
let first = job.bounded(state, 1);
let second = job.unbounded(state, 2);
```

`bounded(...)` and `unbounded(...)` both launch the work. The compiler checks
the selected mode and its arguments statically. An unbounded launch that does
not meet its access and lifetime requirements is an error at that launch.

A blueprint is distinct from the handle returned by a launch. The blueprint
describes work and may be used to start more than one execution. Its captures
and their ownership rules determine whether a particular blueprint can be
reused.

### Context sugar

`spawn` and `detach` are standard-library/context sugar, not core keywords or
modifiers. Each creates a blueprint and launches it, returning a handle:

- `spawn Work(args...)` is the bounded convenience form.
- `detach Work(args...)` is the unbounded convenience form.

The returned handle can track the launched process. It is not the blueprint
and is not merged. The exact spelling of the unbounded sugar (`detach` or
`detached`), the handle's public name, and bounded completion/resource-release
semantics remain open.

## Closures And Callable Values

The proposed closure form is:

```zith
fn |captured values...|(arguments...) {
    // body
}
```

A closure is an anonymous function together with a tuple of captured data.
Capture data may be extracted, but extraction is not required for ordinary
invocation or for passing the closure as the `work` value to `Thread.job`.

This means a blueprint may contain a closure's capture tuple. Reusing that
blueprint depends on the ownership and copy/reuse rules of the captured values.
The design does not assume that every closure is freely copyable or that every
capture must be moved into the launch arguments.

`Functor` provides call-operator behavior so a value can be invoked like a
function. The precise contract that lets a function, a closure, or another
`Functor` be accepted wherever a function is expected is not settled. The
relationship between closures and `Functor` must preserve the fact that a
closure can carry state and that its capture tuple may optionally be extracted.

`call` is being considered as comptime-only syntax sugar that can inspect a
callable and its argument list together. It is not proposed as a runtime call
operation. The exact comptime contract and syntax are open.

Zith-- macros are not part of this callable model. They are a separate
compile-time syntax facility in the currently compiled subset.

## Revocable Access

Unbounded work needs an explicit revocable-access contract for any borrowed
resource whose parent may revoke access. The direction is to express that
contract as a modifier, not as a source-level `Revokable<T>` wrapper type.
ADR-0034 fixes the modifier's spelling: a `'` sigil before the type, with
`grant` as the long form. The sigil precedes any ownership qualifier except a
bare `T`, so `'view T`, `'lend T`, `'^T`, `'^mut T`, `'%T`, and `'%mut T` all
exist (`'%mut T` is the `'own T` of ADR-0026). The sigil applies only in type
position, and the `unbounded` launch site already marks where revocation
happens, so a call argument carries no revocable annotation. The lexical rule
that separates the sigil from a character literal is recorded in ADR-0026.

The runtime may still lower this contract through an internal proxy or control
block. That implementation detail does not require a wrapper type in source
syntax.

## Relationship To Existing Decisions

- [ADR-0015](../adr/0015-full-zith-thread-fork-merge.md) and
  [the branch protocol](branch-protocol.md) describe the earlier `fork` and
  `merge` surface, where a fork launches and returns a mergeable handle.
- [ADR-0026](../adr/0026-nra-aggregate-nodes-and-revokable-flows.md) describes
  `Thread.spawn(...).bounded/.unbounded(...)` and the runtime proxy. Its
  source-spelling section is superseded by
  [ADR-0034](../adr/0034-nra-surface-spellings.md), which sets the accepted
  spelling to `'` / `grant` before any ownership qualifier except a bare `T`.
- [ADR-0033](../adr/0033-nra-reference-model-and-bind.md) is accepted and
  defines the full-Zith reference surface `&`, `^`, and `%`.

This draft does not silently amend those accepted ADRs. ADR-0034 reconciles
the revocable-access spelling. The thread surface, handle lifecycle, and
callable contract still need explicit reconciliation before the draft becomes
an accepted contract.

## Open Questions

- Does a job blueprint containing a closure remain reusable only when its
  capture tuple is copyable, or can it expose an explicit clone/relaunch rule?
- Are closures themselves `Functor`, or do functions, closures, and Functors
  satisfy a separate common callable contract?
- What does "accepted wherever a function is expected" mean for generic
  parameters, function types, and indirect calls?
- What is the exact comptime syntax and semantic input for `call`?
- How does a bounded launch finish and release its borrowed resources if its
  handle is observational and is not consumed by `merge`?
- What can the monitoring handle observe, and can it read a completed result?
- Is the unbounded context sugar spelled `detach` or `detached`?
- The revocable-access modifier composes with every qualifier except a bare
  `T` (ADR-0034), so `'view T` / `'lend T` / `'^T` / `'^mut T` / `'%T` /
  `'%mut T` are resolved. The remaining open point is how those revocable
  forms interact with thread launch and merge.
- How should the accepted reference model disambiguate `&T` from expression
  forms that older drafts use for address-taking or ownership transfer?
