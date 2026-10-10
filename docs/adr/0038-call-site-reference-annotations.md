# Call-Site Reference Annotations

Status: accepted as a full-Zith design direction. Nothing in this decision is
implemented or changes Zith--.

## Context

ADR-0033 fixed the reference model in type position: `&T` and `&mut T` are
references, `^T` and `^mut T` are binds, and `%T` and `%mut T` are owns.
ADR-0034 fixed the spellings and made the sigil and keyword forms equivalent in
type position.

Type position is not the only place a transfer mode appears. A call argument
that is a plain binding defaults to a value move, so `f(x)` and `f(&x)` look
alike unless the language makes the difference visible. This ADR fixes the
call-site surface: an argument whose mode is not a plain value move carries a
sigil that names the mode the callee receives.

## Decision

A call argument carries a sigil that names its transfer mode. The sigil
matches the mode of the callee's parameter, not the declared form of the
argument binding.

| Argument form | Meaning |
|---|---|
| `x` | Plain value move. The default, written with no sigil. |
| `&x` | Read borrow. The callee receives a `&T`. |
| `&mut x` | Write borrow. The callee receives a `&mut T`. |
| `^x` | Read bind. The callee receives a `^T`. |
| `^mut x` | Write bind. The callee receives a `^mut T`. |
| `%x` | Logical move. The callee takes ownership of the slot. |

The annotation reflects the callee's parameter mode. A `^` binding passed to a
`&` parameter is written `&x`, because the free `^ -> &` coercion happens after
the argument is read. The same rule holds for `^mut -> &mut`.

The sigil applies to an argument that names a place: a binding, a field path,
or an index. A literal, a temporary, or a call result is already a value, so it
is a plain value move and carries no sigil. A method receiver is governed by
its declaration and does not repeat a sigil at the call site.

Omission is an error. Passing a place to a parameter whose mode is not a plain
value move requires the matching sigil, and a sigil that does not match the
parameter is rejected. This follows the existing Zith-- rule for `lend x` /
`view x` (E4005), extended to every non-default mode.

A plain value move stays unmarked. The one move that must be marked is a
logical move out of a slot, because it is the surprising one: moving a `%mut T`
out of a struct field or an aggregate slot consumes that slot, so
`f(%s.field)` names the logical move at the call site. The diagnostic for the
later use of the consumed slot names the move site and the provenance of the
moved value.

## Consequences

- `%` gains a call-site use beyond its `%T` type spelling. In type position it
  still names an own; in argument position `%x` names a logical move.
- The call-site surface has one rule: every mode that is not the plain value
  move has a sigil, and the plain value move does not.
- `^` and `^mut` remain legal parameter modes in the language. The universal
  API style discourages them in the public surface; see
  [ADR-0032](0032-universal-api-project-identity-and-contexts.md).
- Zith-- is unchanged. It keeps its conditional `lend x` / `view x` rule and
  does not use the sigil annotations.

## Considered Options

Marking every argument, including the plain value move, was rejected: the plain
move is the default and marking it adds noise without information. A
diagnostic-only rule was rejected: the move out of a slot is common enough that
naming it at the call site is worth the one sigil.
