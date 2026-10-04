# Static Interfaces and Trait Composition

Status: accepted for the full-Zith specification. Zith-- keeps its separately documented structural-interface and `dyn Interface` implementation.

Full Zith treats interfaces as structural static contracts on types and values,
not as behavior traits or dynamic-dispatch types. A type satisfies an interface
automatically when its conjunctive conditions hold. Interface fields guarantee
existence and type. Unqualified fields follow the mutability available through
the value, `let` fields remain immutable while the contract is active, and
`var` fields require mutable storage. Writes still require a mutable access
mode. `view` never permits interior mutation, including for `var` fields.
`var` does not imply synchronization or relax cross-thread safety requirements.
Method requirements use exact signatures and dispatch statically. Header
`requires` clauses constrain `Self`; value-level `self` conditions are checked
at the call boundary and remain invariants while the contract is active. An
unproven `@ensure` is a compile-time error, not a runtime check.

`requires Interface` constrains a trait's `Self` and propagates that interface
bound wherever the trait is required. Generic bounds combine interfaces and
traits explicitly with `+`. Trait clauses appear on separate lines beneath the
name, with `extends` before `requires`. `extends` composes traits only.
Composition combines method requirements, defaults, and capability identity
without inheriting fields. Shared origins in a diamond count once. Identical
signatures are one requirement, conflicting defaults require an explicit
`#[override]`, and an override replaces the inherited default. Implementing a
composed subtrait does not require separately implementing its supertrait or
capability.
