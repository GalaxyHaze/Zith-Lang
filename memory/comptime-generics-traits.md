# Comptime, Generics, Traits & Capabilities Context

Short contextual pointer for the full-Zith 0.7.0 proposal. The detailed
planning and implementation guides now live in
`docs/plans/archive/0.7.0-zith/`; this note only keeps the boundaries that are
easy to confuse when editing the active plan.

The current compiler product split keeps comptime/introspection/type
construction/capabilities in the full Zith spec. The `Zith--` active plan now
lives in `docs/plans/0.7.0/`, and the detailed full-Zith steps were moved to
`docs/plans/archive/0.7.0-zith/`. The traits, interfaces, generic constraints
and monomorphization pieces that were described as future in this note are
implemented in Zith-- today; see `docs/impl-status.md` before calling any of
them pending.

## Locked Scope

- Static dispatch only. `dyn Trait`, vtables and object safety are deferred to
  a later release, but the dedicated "dynamic dispatch is not implemented yet"
  diagnostic is part of 0.7.0.
- Real generic monomorphization before HIR, restoring the documented pipeline
  `sema -> comptime/solve -> NTA/NRA -> HIR`.
- Traits and interfaces become semantically real: trait bodies, default methods,
  `requires`, `extends`, structural interfaces, and conformance checking.
- Static introspection: `@fields`, `@hasTrait`, kind predicates, comptime
  constants for layout intrinsics, and comptime-unrolled `for (f in @fields T)`.
- Full CTFE including `const { }`, `const fn`, and type construction with
  `@struct` / `@appendField` / `@removeField` / `@appendMethod`.
- Capabilities ship as a base only: registry, reserved names, shape validation,
  and an empty behavior slot. No capability changes compiler behavior in 0.7.0.
- `Copy` is not a capability. Implicit bitwise copy is a type-system property,
  not a trait.

## Durable Architecture Facts

`GenericInstantiationPass` in `src/comptime/generic-instantiate.*` runs in
`semaStage()` before `nraStage()` and monomorphizes generic functions, structs,
aliases, methods, and `implement` blocks via
`src/session/compilation-session.cpp`. The pass owns `E3010` (wrong generic
arity), `E3011` (uninferable generic parameter), and `E3012` (instantiation
explosion). Its concrete instances and call-site mappings feed `HirLowerModern`,
which emits only concrete functions with mangled names such as `id<i32>`.
Cached artifacts store an `InstantiationRecord` summary plus per-HIR-function
`instance_index`; cold and warm builds reproduce the same monomorphized HIR.
The cache `Store` must be created only after `FrontendContext` exists because
the warm-key identity depends on it.
See the archived step files under `docs/plans/archive/0.7.0-zith/` for the
original dependency order and capability extension details.
