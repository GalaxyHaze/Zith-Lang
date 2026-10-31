# Forward Dispatch

Status: drawing

Size: one unit in Zith--, one later unit in full-Zith for the enum level.

## Problem

A call site often needs to run one overload of a function according to the
runtime tag of a union or `opaque` value. Today the caller writes that
dispatch by hand as a `when` over the value, one arm per member. The hand
written form is verbose, and nothing forces the arms to cover every member.

Forward dispatch is the call-site construct that removes that boilerplate. It
selects the overload whose parameter matches the tag of one marked argument
and calls it. Because the compiler knows the member set, it can require the
overload set to cover every member, so adding a member turns an incomplete
dispatch into a compile error instead of a silent fallthrough.

The construct is sugar. It erases to the `when` the caller would have written,
so HIR and codegen stay unchanged.

## Non-Goals

- No `enum` dispatch in this unit. The enum level is a state mechanism for
  full-Zith and belongs to a separate unit. It does not require `enum: union`.
  A plain numeric enum can dispatch per state once refined comptime gives each
  state a distinct type, so `fn handle(s: Ok)` and `fn handle(s: Fail)` become
  distinct overloads. That refined comptime is what Zith-- does not have.
- No multi-subject dispatch. Exactly one argument carries the marker. The
  product of several subjects is out of scope for this unit.
- No change to plain `when` exhaustiveness. Making `when` over a union
  exhaustive is a separate decision with its own ADR.
- No runtime semantics of its own. There is no dispatch table, no vtable, and
  no new HIR node.
- No named union variants. The member set is the union's positional member
  type list, which is what the parser accepts today.

## Decisions

These decisions are settled for the drawing. Each one has a named reversal
because the drawing is not yet signed.

1. **Canonical name: forward dispatch.**

   Chosen: the construct is called forward dispatch. The glossary records
   `forward overload`, `forward deduction`, and `tag dispatch` as avoided
   synonyms.

   Reversal: a later slice shows the construct is better named for its
   mechanism than for its operation, and the glossary term is renamed.

2. **Spelling: a `~>` marker on the argument.**

   Chosen: the marked argument is written `work(~> val)`. `~>` is already a
   two-character token consumed only in a `when` case, and `work(~> val)` is
   an argument position, so the parser change is local and unambiguous. The
   deprecation of `~>` in `when` frees the token as the new form lands.

   Reversal: the coexistence window with the deprecated `when` marker proves
   confusing in practice, and the marker moves to a fresh token.

3. **Exactly one marked argument, and it is the subject.**

   Chosen: one argument carries the marker and is the subject. Every other
   argument is fixed and is passed unchanged to each selected call. The
   subject must be a place (a binding, `self`, or a field path), not a
   temporary.

   Reversal: a real use needs two subjects dispatched together, and the
   construct grows a multi-subject form.

4. **Exact member match.**

   Chosen: a member `M` of the subject type is covered only by an overload
   whose parameter type is exactly `M`. Implicit coercions do not count as
   coverage, so a member is never hidden behind a conversion.

   Reversal: exact matching proves too strict for a common member whose only
   sensible overload is reachable through one documented coercion.

5. **Generic overloads instantiate per uncovered member.**

   Chosen: a concrete overload is preferred for its member. A member with no
   concrete overload falls back to a generic overload `fn work<T>` and is
   instantiated with `T = M`. If no concrete or generic candidate exists, the
   member is uncovered.

   Reversal: per-member generic instantiation proves to be the common case
   that the construct exists for, and the drawing promotes the generic path
   over the concrete one.

6. **Ambiguity is an error, with no ranking.**

   Chosen: two candidates that accept the same member are ambiguous and
   report the existing `E2008`. There is no conversion-quality ranking, which
   matches overload resolution today.

   Reversal: a ranking rule is introduced for overload resolution as a whole,
   and forward dispatch inherits it.

7. **Exhaustiveness is enforced with its own code.**

   Chosen: every member must be covered. An uncovered member is a
   compile-time error with a dedicated code, distinct from the `when`
   non-exhaustive diagnostic, so the LSP can tell the two apart.

   Reversal: the dedicated code proves redundant with the `when` code and the
   two diagnostics merge.

8. **`opaque` requires a default.**

   Chosen: an `opaque` subject is an open set and cannot be enumerated, so it
   requires a default body that receives the raw `opaque` value. The default
   is written as the final `(_)` arm of the erased `when`, where the body can
   extract a member with `val as T`.

   Reversal: the project gives `opaque` a closed tag set that the compiler can
   enumerate, and the default becomes optional.

9. **Erase to `when` in sema.**

   Chosen: forward dispatch lowers to the equivalent `when` expression during
   semantic analysis, reusing the existing narrowing and `when` lowering. The
   only new semantic work is the coverage check.

   Reversal: the erased `when` cannot carry a diagnostic or a narrowing the
   construct needs, and the construct lowers to its own HIR.

10. **Zith-- scope, enum level deferred.**

    Chosen: the Zith-- construct covers tagged `union` and `opaque`. The enum
    level is a separate full-Zith unit. It is one construct across both, with
    a tag mechanism in Zith-- and a state mechanism in full-Zith.

    Reversal: the enum level lands first and the Zith-- tag level follows.

## Next Step

Take the drawing through review. On acceptance, compute the promises, then
sign them in an ADR that points to this drawing. Do not write compiler code
before the promises are signed.

## Technical Facts

`~>` is a two-character operator token, consumed only in a `when` case by
`frontend-stmt.cpp`. In expression position `~` is already a unary operator,
so `work(~> val)` is a new argument parse with no ambiguity against `when`.

The `when` construct lowers through `lowerWhen` in `hir-lower-block.cpp`. It
evaluates the subject once into a slot and branches per case. A `(f is Member)`
case narrows reads of `f` to the member type.

Tagged unions store a positional member type list in `UnionDef::members`. The
parser accepts only the positional body `union Name { T, U }`, so named union
variants are not part of the member set. An `enum` today stores one shared
underlying type and named discriminants, so its states are not distinct types
to match. The enum state mechanism needs refined comptime to give each state
its own type; that is a full-Zith unit, not a Zith-- one.

Bare `opaque` is a tagged open union stored as `{ *void, u32 }`. `opaque is T`
compares the typeId and narrows the checked local to `T` in the then branch.
`opaque as T` returns `?T` with a checked extraction.

Overload resolution selects by parameter count and types, prefers an exact
signature, and reports `E2007` when no candidate accepts the arguments and
`E2008` when more than one does. Return type never participates. The resolved
overload is recorded and reused by generic instantiation.

The `when` non-exhaustive diagnostic today is `TypeMismatch` with the message
`non-exhaustive when; add a default case '(_) ...'`, raised in
`sema-cast-coerce.cpp`. It is the model the dedicated forward dispatch code
must stay distinct from.
