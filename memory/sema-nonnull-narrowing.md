# Non-null Narrowing Proofs And Their Invalidation

Zith-- proves a `?*T` (and other optional-pointer) local non-null inside an
`is null` / `not (is null)` branch and early-return guards. The proof is stored
per `LocalId` in `TypedMap::provenNonNullExprs` (`memory::FlatSet<uint32_t>`,
`src/sema/sema-modern.hpp`) and consumed by `exprHasNonNullPointerProof` in
`src/sema/sema-cast-coerce.cpp`. The deref/arrow/index checks in
`src/sema/sema-expr.cpp`, `src/sema/sema-index.cpp` require that proof and
report `E3005` otherwise. See `docs/impl-status.md` for the user-facing status.

## Proofs are invalidated by a write to the binding

`inferAssign` (`src/sema/sema-assign.cpp`) erases the local's
`provenNonNullExprs` entry whenever the assignment target is a bare binding
name (`ExprKind::Name`). This closes a real soundness hole: previously a local
narrowed to non-null kept the proof after `p = q;`, so `p->field` still
compiled and segfaulted at runtime when `q` was null.

The regression is `nonNullNarrowingIsInvalidatedByReassignment` in
`tests/test-memory-qualifiers.cpp`. It asserts three things: the proof does not
survive the reassignment, the dereferenced reassigned pointer reports `E3005`,
and re-narrowing after the write is accepted again.

## Scope and remaining debt

The invalidation covers writes to a plain binding name. It does not yet track
derived paths (`self->table->occupied`) or cross-function proofs, so narrowing
still follows only a local binding, not a field/arrow path. That is the
documented `?*T pointer narrowing is local-only` debt in
`docs/impl-status.md`, a sema hardening item and not a std-lib blocker.

`is null` remains the only flow-narrowing mechanism (see the `must`/`raw`
optional-extraction row in `docs/impl-status.md`).

## Optional aggregate narrowing (`?S`, `?pack`, `?i32`)

The same `is null` early-return guard also narrows **optional aggregates**, not
just `?*T`. After `if (x is null) { return ...; }` the reads `x.field`,
`x.len`, and `return x;` (for a `?i32` result) are accepted, matching the
pointer niche behavior. The proof is still per-`LocalId`, but the payload is
extracted at the use site instead of the name site:

- `PerModuleSema::provenOptionalAggregatePayload`
  (`src/sema/sema-cast-coerce.cpp`) returns the stripped `opt->inner` only when
  the local carries the non-null proof and the inner is **not** a pointer (the
  pointer case keeps unwrapping through `inferArrow`).
- `coerceValue` records the payload in `TypedMap::optionalPayloadReads` and
  substitutes the source type, so `return x;` coerces the payload for a
  non-optional result.
- `inferField` (`src/sema/sema-index.cpp`) records the same marker and uses the
  payload as the field base type for `x.field` / `x.len`.
- `lowerName` (`src/sema/hir-lower-expr-value.cpp`) reads the marker and emits
  `HirField{SlotAddr{slot, ?T}, 0, payload}` in `Guarded` mode, extracting field
  0 of the stored optional aggregate. `hir-verify` accepts the guard because
  `lowerIsNull` reuses the local's slot, so the dominating check matches.

The name itself must stay optional (`x` keeps type `?T`) so postfix `x?`
propagation still works; only the field-access and coercion sites unwrap.

## `null` member in a pack literal (resolved)

A pack literal member `null` with no type context used to infer the
`null`/`never` type, so `|null, 7u64|` carried a `void` member and erasing it
into an `opaque` parameter dropped the whole value (the callee read garbage or
the backend failed to emit a terminator). The fix has two parts:

- `inferPackLiteral` (`src/sema/sema-literal.cpp`) now takes the surrounding
  annotated pack type. `inferExpr` passes the expected result recorded by
  `inferExprExpected`, so `let a: |bytes: ?*u8, len: u64| = |null, 7u64|;`
  retypes the bare `null` to `?*u8` and the aggregate keeps its concrete
  layout. One optional layer is looked through, so `?|...|` also supplies the
  member types.
- `null as ?T` is now a valid cast (`inferCast` in `sema-cast-coerce.cpp` and
  `lowerCast` in `hir-lower-expr-value.cpp`): it fixes the optional type of a
  bare `null` and lowers to a `None` of the target type. This is the explicit
  escape hatch for a call-site literal such as `new<Buffer>(|null as ?*u8,
  0u64|)`.

When neither applies, `inferPackLiteral` itself now reports `E3001` ("a bare
'null' in an untyped pack literal has no member type") and returns `error_type`,
so the malformed pack is never interned. This is the single rejection point:
the earlier guard in `coerceValue`'s opaque-erase branch only fired when the
literal was erased into `opaque`, so a directly bound literal such as
`let t = |null, 7u64|;` still reached LLVM codegen with a `void` member and hung
at 100% CPU in `codegen-type.cpp` (`lower(TypePack)` building a struct with a
`void` field). Rejecting during inference stops every downstream path.
Regression: `test_null_member_in_pack_literal_runtime` in `tests/test-codegen.cpp`
covers the annotated binding, the explicit `null as ?*u8`, the rejected opaque
case, and the directly bound literal that used to hang codegen.

## Aggregate literals coerced to `opaque`

An implicit `T -> opaque` coercion records the concrete source type in
`TypedMap::opaqueSourceTypes`, but the whole expression is typed `opaque`. For
a **literal** the lowering already restored the concrete type; for an
**aggregate literal** (struct/pack/array) it did not, so `emitMakeOpaque`
spilled the value with the opaque `{ *void, u32 }` layout. A two-field struct
happened to match the wrapper's 16-byte size, but a one-field struct stored the
wrong width and segfaulted. `lowerExpr`
(`src/sema/hir-lower-expr.cpp`) now applies the concrete-type lookup to the
`Literal`, `StructLiteral`, `PackLiteral`, and `ArrayLiteral` cases. Regression:
`test_pack_literal_erased_into_opaque_runtime` in `tests/test-codegen.cpp`.
