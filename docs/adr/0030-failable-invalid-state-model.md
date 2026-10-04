# Failable Invalid-State Model

Status: accepted for the full-Zith specification. Implementation remains spec-only.

## Context

Zith needs one model for absence and other invalid values without treating every
invalid outcome as an error. `Nil` represents absence, while types can define
their own invalid states and error values. Compiler-managed handling must let
users choose between a fallback, local inspection, and propagation.

## Decision

- `Nil` is the universal absence state. It is invalid, but it is not an error
  and does not implement `Error`.
- `Failable` defines the contract the compiler uses to distinguish a value's
  valid and invalid states. Users do not call `check()`, `valid()`, or
  `invalid()` directly.
- `try x` evaluates `x` once, calls `check()` once, then calls exactly one of
  `valid()` or `invalid()`. Its result is local, can be stored and tested later
  with `is @ok`, and is not automatically propagated out of the function.
- `or` evaluates its fallback only when the preceding result is invalid. It
  does not pass that invalid state to the fallback. If every alternative is
  invalid, the local result retains the last invalid state.
- Postfix `!` propagates an invalid state to the enclosing function.
- `fail` handles only invalid values whose types implement `Error`.
- `catch` handles invalid values produced while initializing `with`, not
  invalid values produced by the `with` body.

## Consequences

- Absence can be handled as invalid without being treated as an error.
- `try` and `or` do not force propagation. Code can retain an invalid result
  and branch on it later.
- `fail` cannot intercept `Nil` or other invalid values that do not implement
  `Error`. `catch` remains scoped to `with` initialization.
- The full-Zith contract remains specification-only. Marker-only capability
  syntax and the exact `with`/`catch` handler grammar remain open.
