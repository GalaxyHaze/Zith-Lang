# Optional dyn Niche Representation

Status: accepted

`?dyn Trait` uses the same two-word fat pointer as `dyn Trait`, with a null
data pointer as the `null` state, instead of a tagged `{dyn, i1}`. The niche
keeps `?dyn Trait` and `dyn Trait` layout-identical, so `@sizeOf(?dyn T) ==
@sizeOf(dyn T)`, and avoids extracting the payload on every field, call, return
and assignment read. A tagged layout is simpler in the type lowerer but forces
that extraction everywhere and doubles the size of the slot.

The niche matches the existing `?*T` treatment, which already uses a null
pointer as `None`. Dispatch on a possibly-null `?dyn Trait` is an error until an
`is null` narrowing proves the payload present.

This signs the representation decision in the
[optional dyn receiver plan](../plans/optional-dyn-receiver.md).
