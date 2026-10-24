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

**Consumer readiness (prepared, not implemented):**

| Consumer | Required before implementation |
|---|---|
| VIR | An executable HIR-to-VIR consumer; verified HIR input; independent, nonzero VIR-schema and VM-semantic versions; mismatch regenerates VIR without invalidating compatible HIR; runnable reuse/miss fixture. |
| Codegen | A concrete self-Zith backend; inputs available from shared HIR or an approved contract extension; target-specific outputs remain outside shared HIR/VIR artifacts; runnable output fixture. |

Do not add placeholder VIR or codegen modules, or claim that version gates
perform regeneration. LLVM IR, objects, assembly, and other target-specific
products remain outside shared HIR/VIR serialization.

Refactor the shared modules in `../Zith` first, validate both HIR adapters and
cache compatibility, and only then port the stable interfaces and behavior to
this repository. This repository remains the Zith project target; Zith-- is a
bootstrap language, not the definition of Zith.

**Declaration and template contract (accepted 2026-10-24):** HIR owns explicit
declaration-level `is_extern`, optional `body_function_id` and `template_id`
references, a template table, and separate ordered method-declaration lists.
Optional references use the corresponding invalid-ID sentinel; names,
`HirFunction.foreign_c`, and general declaration `members` are not substitutes
for semantic links. Each template owns its declaration identity, generic
parameters and bounds, signature facts, fields and canonical order, methods,
and trait references; an optional function link is additional identity, not
the source of those facts. Trait references use an explicit HIR Trait type
instead of collapsing to Unknown. These HIR changes require schema 2; adding
Trait to Metadata's type-kind domain requires Metadata schema 3. Decls remains
schema 2. Templates advances from schema 1 to schema 2 because each template
record must persist its explicit declaration ID and optional function ID;
omitting either would make the accepted HIR contract impossible to hydrate
without inferring a relation. Attrs remains schema 2 and the container remains
version 21.
