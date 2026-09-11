# Concrete Generic Type Identity Is Structural And Name-Derived

Concrete generic types in Zith-- get their identity from the substituted
layout, then use the substituted type name as the stable key that maps the
concrete type to that same layout across a session and across cache
hydration. The old heuristic, which derived `StructType` names from pre-
substitution fields, mis-identified nested instances such as
`Entry<K, V>` under `HashMap<K, V>`. This ADR records the accepted identity
rule: the generic parameter declaration is the source of truth for bound
propagation, while a reified `StructType` keeps its concrete type arguments as
metadata for method lookup and cache round-trips.

Status: accepted

## Accepted Rule

1. A reified struct, enum, union, alias, or nominal type has a concrete
   substituted layout and a concrete name. The name is derived from the
   substitution, not from the template's pre-substitution fields.
2. `StructType` stores the concrete type arguments that produced the reified
   struct as metadata alongside the name, fields, field names, and field
   metadata.
3. The base name of a concrete type stays the owner key for method lookup, so
   `HashMap<u64, u64>` and `HashMap` methods resolve through the same
   `HashMap` declaration.
4. The concrete type arguments stored on `StructType` replace the heuristic
   owner-argument reconstruction in `sema-method.cpp` for method calls on
   concrete struct receivers.
5. `GenericBinding` remains the source of truth for bounds. When a generic
   field is read from an owner, the owner's `GenericParam` is preserved so
   `boundsForGenericParam` can resolve `K: Hashable` through the field.
6. The cache serializes the same concrete type arguments on
   `CompactStructDef`, so a hydrated `HashMap<u64, u64>` preserves the
   identity rule without relying on parsing the mangled name.

## Considered Alternatives

Name-only identity was rejected because the name alone could not distinguish
the pre-substitution template from a concrete instance and did not carry the
receiver arguments needed by method resolution. Field-scan heuristics were
rejected because they produced `Entry<T, T>` for `Entry<K, V>` and are
redundant once `StructType` owns the concrete arguments.
