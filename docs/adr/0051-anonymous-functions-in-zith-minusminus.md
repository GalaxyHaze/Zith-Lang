# Anonymous Functions in Zith--

Status: accepted for the Zith-- iteration.

Zith-- gains a function literal, an anonymous function written in expression
position and used as a value. The first form carries no captures, so it is a
plain function value rather than a closure.

## Context

Zith-- already has the function value type `fn(...): R` and accepts a
reference to a named, non-generic function wherever that type is expected
(`docs/impl-status.md`). The named reference lowers to the function's linkage
name and an indirect call goes through the existing C function-pointer ABI.
There is no function literal: `parsePrimary` in
`src/frontend/frontend-expr-primary.cpp` has no `fn` atom, so
`let f = fn (a: i32, b: i32): i32 { a + b };` fails to parse with `E0001`.

The full-Zith callable model in
`docs/plans/callable-thread-blueprints.md` proposes a closure form,
`fn |captured values...|(arguments...) { ... }`, where a closure is an anonymous
function together with a captured tuple. That model carries state and is out of
scope for Zith--.

## Decision

Zith-- adds an anonymous function literal with an empty capture list. The
literal is written `fn (params): R { body }` or `fn (params) { body }`, the same
shape as a named function header without a name. The `|...|` capture group is
not part of the Zith-- form. Because there are no captures, the value is a
plain function value: no environment struct, no capture tuple, no closure ABI.

The literal is represented as a synthesized local function whose address is
taken, exactly as a named function reference is today. Frontend recognizes `fn`
as a primary expression and produces a dedicated expression kind. Sema types the
literal as a `FunctionType` and checks the body in the enclosing scope. HIR
synthesizes a function with a deterministic mangled name and emits the value as
a reference to it. Codegen and the indirect-call path are inherited unchanged.

## Considered Options

- **Add closures with captures now.** Rejected for this iteration. Captures
  pull in an environment representation, capture-tuple ownership, and the
  callable/Functor contract that
  `docs/plans/callable-thread-blueprints.md` still leaves open. The
  no-capture form delivers the useful case without that surface.
- **Require a named helper function.** Rejected: it forces a top-level
  declaration for a value that is only used once, which is the case the literal
  exists to serve.
- **Reuse the `TypeSum`/closure machinery.** Not applicable: a no-capture
  function value is a single function pointer, not a sum or a fat pointer.

## Consequences

- The Zith-- callable surface becomes "named function reference plus anonymous
  function literal, both no-capture", consistent with the existing
  "No closures or captures" note in `docs/impl-status.md`.
- `docs/06-mutability-bindings.md` states that packs "are not anonymous
  functions". That statement stays true: packs are aggregates, and the new
  literal is a function value. The wording does not need to change, but a reader
  should not read it as "Zith-- has no anonymous functions".
- `docs/Zith--.md`, `docs/Zith---implementation.md`, and `docs/impl-status.md`
  must describe the new expression form when it lands.
