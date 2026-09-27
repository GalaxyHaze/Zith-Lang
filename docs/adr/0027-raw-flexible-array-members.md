# Raw Flexible Array Members

Status: proposed

Zith will reserve `raw []T` as a future representation for a C-compatible
flexible array member, not as an unchecked ordinary slice. In this form, `raw
[]T` is allowed only as the final field of a foreign struct or another
explicitly raw layout declaration and denotes an inline tail whose storage and
length are supplied by the containing allocation or by an external ABI
contract. Ordinary `[]T` remains a
pointer-plus-length slice with Zith's normal ownership, lifetime, and bounds
semantics.

The feature is primarily intended for C interop and binary layouts. It is not
part of the current validated C ABI surface and must remain rejected until the
binder and each supported target can prove its layout and access rules.

## Considered Options

- Model every C flexible array as an ordinary `[]T`. Rejected because that
  changes the representation to a fat slice and loses the inline-tail ABI.
- Replace `DynArray<T>` with flexible array members internally. Rejected because
  compiler-owned collections need resizing, element lifetime management, and
  arena integration rather than a fixed-size-at-allocation tail.
- Expose flexible arrays as unrestricted language types. Rejected because
  `sizeof`, copying, assignment, by-value passing, and lifetime do not have the
  semantics of ordinary complete Zith values.
- Keep rejecting the layout permanently. Rejected because valid C APIs and
  binary formats use flexible array members and should eventually be bindable.

## Consequences

- `raw []T` is distinct from `[]T` in the type and layout system even though the
  surface syntax is related.
- A flexible-tail struct has a complete fixed header size, but its tail is not
  included in ordinary value copying or by-value ABI.
- Flexible-tail structs cannot be copied as normal values, passed or returned
  by value, embedded in ordinary structs, or used as elements of arrays.
- Access to the tail requires an explicit length from a header field or another
  trusted ABI contract; the tail does not carry a length by itself.
- Converting the tail to an ordinary `[]T` must be explicit and must preserve
  the pointer escape and lifetime rules.
- The C binder must retain target-specific tail offset, element layout, and
  alignment metadata and must emit a clear diagnostic when those facts cannot
  be validated.
