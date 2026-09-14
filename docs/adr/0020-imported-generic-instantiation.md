# Imported Generic Structs And Methods Reuse The Local Instantiation Path

Generic declarations imported through a module alias must be reified from the declaring module's template, not from the current module snapshot. Qualified struct literals, type annotations, field defaults, and generic methods on imported concrete receivers use the same `StructType.args` and generic instantiation machinery as local generics; missing method type arguments are inherited from the concrete receiver. This is required before `DynArray<T>` or an expanded `HashMap<K, V>` becomes a consumable stdlib surface.

Status: proposed

Considered options:

- A separate imported-template instantiation path was rejected because it would duplicate reification, bound propagation, `bindCall`, and cache identity handling, and would drift from the local generic behavior.
- A docs-only contract claiming generic maps are consumable was rejected until the compiler can instantiate them from another module.

Consequences:

- `import std/collections/hash_map` should support `hm.HashMap<i64,i64>{}`, defaults from the declaring module, and `hm.HashMap.init(lend map)` without repeating type args.
- `stdlib/std/collections/hash_map.zith` should be promoted to a consumable checked generic map only after this path is proven by tests.
- `DynArray<T>` remains after this fix, not before it.
