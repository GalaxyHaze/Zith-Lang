# Shared HIR and cross-dialect cache

**Status: accepted (2026-10-21).**

Zith and Zith-- remain sibling languages with distinct frontends and semantic
rules. Reuse only HIR, VIR, cache, and codegen; their shared interfaces must not
depend on frontend AST, semantic-analysis, or session types. Each frontend
owns its lowering adapter; the verified HIR module is the shared seam, not a
shared frontend superclass. Each adapter maps compatible constructs to the
same canonical HIR contract, while frontend/source cache entries remain
dialect-specific.

HIR is the canonical cached representation. It records required runtime-check
obligations separately from optional proof facts: `raw` is explicitly
unchecked, a proven-safe operation may elide a check, and missing proof facts
must retain the check or trigger proof recomputation. Persisted identities are
canonical rather than process-local. Unknown optional metadata may be ignored
only when doing so preserves semantics; unknown required semantics invalidate
the artifact.

VIR is derived from HIR and may be reused only when the VM semantic and VIR
schema versions match. Backend-specific outputs are not part of the
cross-dialect cache contract. Container, HIR-schema, and VIR-schema versions
are tracked independently. This narrows the artifact-sharing claim in
[ADR-0006](0006-cache-is-transversal-materialized-at-the-end.md); its
transversal cache lifecycle remains in force.

Refactor the shared modules in `../Zith` first, validate both HIR adapters and
cache compatibility, and only then port the stable interfaces and behavior to
this repository. This repository remains the Zith project target; Zith-- is a
bootstrap language, not the definition of Zith.
