# Explicit Alignment and Parser Attributes

Status: accepted

Zith-- uses `#[alignas(N)]` to strengthen ordinary struct layout or one
permitted binding's storage, with target support and exact C ABI layout
required at interoperability boundaries. This signs `ABI-ALIGN-01` through
`ABI-ALIGN-03` in the [alignment plan](../plans/abi/explicit-alignment.md).
The companion parser change recognizes `#[noalias]` only on functions with
pointer return types, and recognizes `#[likely]` and `#[unlikely]` without
promising optimizer behavior.
