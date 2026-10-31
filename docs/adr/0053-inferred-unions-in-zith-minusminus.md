# Inferred Unions in Zith--

Status: accepted for the Zith-- iteration.

Zith-- infers a tagged union from the branches of a `when` expression when the
expected type is the bare `union` keyword. An inferred union is identical to a
named tagged union, except that the compiler generates the declaration. Union
identity is structural: the same member set is the same type everywhere.

## Context

`docs/03-type-system.md` shows the bare `union` hint:

```zith
let value: union = when (flag) {
    A = 42,
    B = 3.14,
    C = true,
};
```

The compiler rejects it with `E2001 unknown type 'union'`. There is no
`TypeExprKind` for bare `union` in `src/frontend/frontend.hpp`, so it falls
through to a name lookup. `inferWhen` in `src/sema/sema-cast-coerce.cpp` also
requires every case body to have the same type, which the heterogeneous example
violates.

Two internal representations are relevant. `TypeSum`
(`src/types/type-kind.hpp`) is structural but has no codegen: it falls through
to a pointer in the `std::visit` in `src/codegen/codegen-type.cpp`. `TypeUnion`
with a `UnionDef` has full codegen (tagged layout, `HirUnionCast` construction,
`HirUnionCheck` narrowing, `as` member validation), but its canonical identity
includes the defining module and the name
(`src/types/type-canonical.cpp`), so it is nominal, not structural.

## Decision

When the expected type of a `when` expression is the bare `union` keyword, the
compiler collects the distinct types of the case bodies and synthesizes a named
tagged union with exactly those members. The same applies to a function return
written `fn f(...): union`, and to any binding annotated `let x: union`. The
bare `union` hint is a placeholder that asks the compiler to generate the
declaration.

An inferred union is the same artifact as a named tagged union. The compiler
generates a `UnionDef` and reuses all of the union machinery, so layout, casts,
narrowing, and codegen are inherited rather than reimplemented. The internal
`TypeSum` is not used as the runtime representation, because it has no codegen.

Union identity is structural. Two inferred unions with the same members are the
same type, in any module. This is implemented by giving the generated union a
name derived only from its members and an empty defining module:

1. Members are ordered by their canonical type id, so the member order, and
   therefore the runtime tag, is deterministic and independent of source order.
2. The generated name is a pure function of the ordered member canonical ids.
3. The generated union has no defining module, and is excluded from the
   defining-module assignment that runs on first lowering, so the module never
   enters its canonical id.

With those three rules the canonical id of a generated union depends only on its
member set, which matches the structural identity of `TypeSum`. The shared name
function is the single point both the generator and the canonicalization agree
on, in the same spirit as `canonicalTypeId`.

## Considered Options

- **Use `TypeSum` and add codegen for it.** Rejected: it would build a second
  runtime representation in parallel with the union one. Generating a `UnionDef`
  inherits the finished union codegen instead.
- **Nominal identity, one generated union per site.** Rejected: it contradicts
  the structural rule. `{f32, u64}` in two places would be two types.
- **Order members by source order.** Rejected: the member index is the runtime
  tag, so source order would make the tag depend on the site and break cache
  hydration. Ordering by canonical id fixes it.
- **Keep the defining module on the generated union.** Rejected: the module
  enters the canonical id and would make two identical unions in different
  modules distinct.

## Consequences

- Both the bare `union` return annotation and `let x: union = when ...` are the
  same feature and ship together.
- `inferWhen` must become expected-type aware, like `inferCall` already is, so
  it can collect member types instead of requiring equal case bodies. This is a
  localized sema change.
- Union identity becomes structural and global. The generated name function and
  the exclusion from defining-module assignment must stay consistent with the
  union hash in `src/types/type-canonical.cpp` and with cache hydration. This is
  the part that is not localized.
- A generated union has no source name, so diagnostics and reflection must
  present it by its member set.
- This depends on the expected-type work in
  [ADR-0052](0052-return-type-inference-in-zith-minusminus.md).
