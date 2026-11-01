# Anonymous States in Zith--

Status: accepted for the Zith-- iteration.

Zith-- supports no-capture state literals as values, so callers can pass state transitions
through values instead of requiring every target to be named in the same machine. This extends
the existing `state(params): R` value model while keeping machine membership, name resolution,
and `self` behavior explicit.

## Decision

A state literal has the form `state(params): R { body }` and produces a value of type
`state(params): R`. It has no source-level name and captures no lexical variables. Its return
type annotation is required.

An anonymous state is valid anywhere a state value is valid, including module scope. Anonymous
states declared in a state body belong to that body's local machine. At module scope, they are
grouped by return type in the same way as named top-level states.

`dock` accepts an anonymous state value. `jump` accepts either a directly resolved state name
or a state value. A value-based jump may target a state from another machine when its state
signature is compatible with the current machine's return type and the supplied arguments.
Named direct jumps retain their existing same-machine rule.

Inside a state, `self` denotes the current state and `jump self(args)` targets it. This applies
to named and anonymous states. In struct and trait methods, `self` remains the special receiver
parameter. A free function has no implicit `self`. Using `self` as an ordinary parameter or
binding name emits a warning.

Name lookup for a `jump` follows lexical binding resolution. The nearest binding wins. If that
binding is not a state value, the jump is an error and lookup does not continue to an outer
binding or state declaration. Name collisions in the same scope are errors.

No-capture `fn` and `state` literals may initialize module-level `const` bindings as static
function addresses. This does not make other expression forms valid global constant
initializers.

## Considered Options

- **Restrict `jump` to named states in the same machine.** Rejected because state values already
  exist, and value-based transitions allow a caller to choose the next state without coupling
  the transition to a particular declaration name.
- **Resolve a `jump` past a non-state local binding to an outer state name.** Rejected because
  silently skipping the nearest lexical binding would make shadowing surprising.
- **Allow anonymous states to capture local variables.** Rejected for this iteration because
  captures require an environment representation and a different state-call ABI.

## Consequences

- `jump` must type-check both direct state names and state-valued expressions, while preserving
  the target's argument checks and the machine return-type invariant.
- Anonymous states need deterministic membership rules: body-local literals join the local
  machine, while module-level literals join the top-level machine for their return type.
- `self` has context-dependent meaning in states and methods, but no implicit meaning in free
  functions. Ordinary bindings named `self` remain diagnosable without changing method receiver
  semantics.
- Global constant evaluation may accept function and state literal addresses, but must not
  broaden the existing constant-expression rules to arbitrary expressions.
