# Explicit Alignment for Zith--

Status: signed

Zith-- needs explicit alignment for layout-sensitive structs such as arenas and
token streams, and for selected local or module-scope storage such as a lexer
scratch array. The design must distinguish a type's layout from one binding's
storage, preserve target ABI correctness, and reject requests the target or C
interoperability boundary cannot prove.

Size: system. The contract crosses declaration attributes, type layout,
binding storage, code generation, and C ABI validation.

## Non-goals

- Field-level alignment, function parameters, or changes to global binding
  forms.
- Type-level alignment on `component`, `union`, or `enum`.
- Generic-dependent alignment expressions.
- Changing allocator APIs or applying a binding attribute to dynamic
  allocations.
- Implementing the compiler in this design task.

## Decisions

- Use `#[alignas(N)]` on ordinary `struct` declarations and on individual
  `let`, `var`, or `const` bindings that Zith-- already permits.
- `N` is a compile-time positive power of two, independent of generic
  parameters. Duplicate attributes, invalid values, and unsupported target
  alignments are errors.
- A type request sets effective alignment to the greater of natural alignment
  and `N`. It affects every instance, `@alignOf`, `@sizeOf`, field offsets,
  containing layouts, and array stride.
- A binding request aligns only that binding's storage. It does not change the
  type, its size or `@alignOf`, and does not transfer through copies or
  assignments. For an array binding it aligns the array's base storage, not
  each element.
- Function parameters and fields cannot carry the binding attribute.
  Dynamically allocated storage remains governed by its allocator and the
  allocated type's alignment.
- At a C ABI boundary, accept an aligned value only when the compiler proves
  the exact target layout. Otherwise reject it with a diagnostic.
- Recognize `#[noalias]` only on functions whose return type is a pointer.
  This parser and semantic validation does not add optimizer behavior.
- Recognize `#[likely]` and `#[unlikely]` without assigning backend semantics.

## Promises

### ABI-ALIGN-01: Type layout

Contract: `#[alignas(N)]` on an ordinary struct raises its effective type
alignment to `max(natural, N)`. The result is reflected in layout intrinsics,
containing structs, and arrays. Generic structs may use the attribute, but its
value cannot depend on generic parameters.

Target and layout obligation: `x86_64-unknown-linux-gnu`.

| Fixture | Field offsets | Size | Alignment |
|---|---|---:|---:|
| `AlignedSmall { value: u8 }`, requested alignment 64 | `value: 0` | 64 | 64 |
| `ContainsAligned { prefix: u8, value: AlignedSmall }` | `prefix: 0`, `value: 64` | 128 | 64 |
| `[2]AlignedSmall` | elements at 0 and 64 | 128 | 64 |

Seam: `test-codegen`, with accepted and rejected attribute syntax in
`test-frontend`.

Reverse: remove the type-level ABI guarantee or change how requested alignment
participates in the target layout.

### ABI-ALIGN-02: Binding storage

Contract: `#[alignas(N)]` on a permitted binding guarantees that binding's
address is divisible by `N`, while leaving the declared type layout and other
bindings unchanged. A copy follows the destination binding's own declaration.

Target and layout obligation: `x86_64-unknown-linux-gnu`; a binding of
`[128]u8` retains size 128 and contiguous one-byte elements while its base
storage is 64-byte aligned.

Seam: `test-codegen`, checking the binding address and unchanged type size.

Reverse: make a binding-level request part of the type or propagate it to
copies and assignments.

### ABI-ALIGN-03: Validation and ABI boundary

Contract: reject non-positive, non-power-of-two, duplicate, generic-dependent,
or target-unsupported requests. Reject the attribute on fields, parameters,
components, unions, and enums. Permit it on local bindings and module-scope
bindings already accepted by Zith--. Cross the C ABI only after exact layout
validation.

Seam: `test-frontend` for attribute diagnostics and `test-cinterop` for
validated versus unverified layouts.

Reverse: broaden the supported declaration sites or accept an unverified C
layout.

## Review Gate

The idea owner approved the unchanged semantic decisions from questions Q1-Q18.
The contract is signed by
[`ADR-0047`](../../adr/0047-explicit-alignment-and-parser-attributes.md).
Conforming tests are drafted before compiler implementation.
