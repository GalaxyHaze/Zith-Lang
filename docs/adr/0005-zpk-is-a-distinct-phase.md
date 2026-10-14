# ZPK is a distinct proof phase

The Zith Proof Kernel is a distinct phase after Sema and before HIR, with the
dependency order NIA -> RRA -> MRA -> NRA: numeric facts are ready before
geometry, geometry validates ranges before ownership, MRA supplies region
identity, and NRA concludes ownership and lifetime.

Keeping the proof out of Sema gives each sub-system its own testable seam and
keeps semantic analysis from accumulating proof semantics. The phase emits a
stable HIR boundary with residual facts (ownership, bounds, region-validated
access) carried in side tables rather than as HIR nodes.
