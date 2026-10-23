# Shared HIR Interoperability: Implemented Work

Status snapshot: 2026-10-23 (end of session)

This document records implementation reported for the plan
`shared-hir-interoperability.md`. It is a progress summary, not a replacement
for the canonical plan. The canonical plan is the source of truth for the
step-by-step work; this file records what has landed and what is still open.

## Implemented in `/home/diogo/Zith`

| Plan area | Result | Commit |
|---|---|---|
| Shared HIR contract | Defined frontend-neutral HIR data and verification, with the required consumer updates. | `02793ab8` `refactor(hir): define verified frontend-neutral contract` |
| Zith-- lowering | Adapted Zith-- lowering to produce the shared HIR contract. | `7d56fe67` `refactor(sema): lower Zith-- into shared HIR` |
| Canonical HIR cache | Added cache-owned identity and persisted canonical HIR through the ZIRL reader/writer. | `6e7e922e` `refactor(cache): persist canonical shared HIR` |
| Derived VIR | Added VIR schema and VM semantic versioning, plus cache reuse/invalidation behavior. | `be334a85` `refactor(ir): version derived VIR` |
| Follow-up correctness fixes | Preserved imported generic layout types and struct type arguments in cached artifacts. | `59377508`, `8c12c9bd` |

Revalidated on 2026-10-23: `/home/diogo/Zith` is clean on `main` at
`8f313dbf`, 22 commits ahead of `origin/main`. Its ZIRL writer uses container
version 21; the self-Zith port now uses that same version.

## In Progress in `/home/diogo/self-Zith`

The port is in Step 6 of the plan, specifically at Step 6.1 (diagnose and
repair the failing typed writer). The following cache foundations and sections
are implemented and committed:

- ZIRL v21 container framing, section validation, and little-endian byte
  primitives.
- Generic artifact-envelope reader/writer for canonical identity,
  dependencies, section tables, payloads, and checksum validation. Complete
  `HirModule` section assembly and restoration remain pending.
- Compact HIR type-entry encoding.
- Metadata schema 2, including strings, paths, types, and nominal definitions.
- Decls, Templates, and Attrs section codecs.
- Compact Code section codec for expressions, functions/blocks, globals,
  vtables, and canonical mappings, using caller-owned decode storage. Function
  records preserve `foreign_adapter_name_id`, and Call expressions preserve
  the variadic-slice fields `variadic_slice_param`, `is_variadic_slice`, and
  `auto_collect_tail`. Container version bumps reject incompatible older
  layouts.
- Artifact-owned canonical type-table foundation: HIR-local type IDs are
  mapped into caller-owned artifact storage, and function signatures are
  synthesized with return/parameter IDs and calling-convention flags. Native
  tests cover the generated signature, storage capacity failure, and invalid
  parameter references. Full artifact orchestration is still pending.
- HIR-to-Code projection: verified HIR expressions, functions, blocks, globals,
  vtables, and canonical mappings project into compact Code records. Local
  type and function identities are remapped, and nested argument, parameter,
  block-instruction, and vtable slices are copied into caller-owned storage.
  Native tests cover projection, Code round-trip, and source-slice mutation
  after projection.
- Typed section-to-container writer: Metadata, Decls, Templates, Code, and
  Attrs payloads are encoded in the required ZIRL order with fixed section IDs
  and schema versions, then passed to the validated artifact-envelope writer.
  This slice is committed (`9d56fe2`) but **not validated**: see
  "Blocked: Step 6.1" below. Success-path layout assertions and the
  undersized-buffer failure test remain incomplete. Envelope reading remains
  covered by the separate container test.
- Caller-owned decode storage and failure handling for the section codecs;
  tests cover golden bytes, round-trips, truncation, invalid values, and
  short output buffers in the implemented sections.

The cache-port baseline before the Code section work was `800a60a`
(`feat(cache): add ZIRL attributes section codec`). The section codecs and
generic artifact-envelope tests pass with the sibling build's compiler. They
cover golden bytes, round-trips, reserialization, truncation,
insufficient-capacity, short writers, malformed records/vtables, and checksum
rejection:

```sh
ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh tests/run.sh
```

## Blocked: Step 6.1 (typed writer returns `InvalidSections`)

Re-run on 2026-10-23 with `ZITHC=/home/diogo/Zith/build/zithc`:
`tests/run.sh` exits 2. Isolating the harness steps shows that five of the six
steps pass and only the last one fails:

| Step | Result |
|---|---|
| `zithc --include <tmp>/src test <tmp>` | 0 |
| `run --virtual-machine --no-cache tests/run.zith` | 0 |
| `run --no-cache tests/canonical-types.zith` | 0 |
| `run --no-cache tests/hir-contract.zith` | 0 |
| `run --no-cache tests/zirl-artifact.zith` | 0 |
| `run --no-cache tests/zirl-hir-artifact.zith` | **2** |

The failing fixture returns `2`, which is the value of
`ZirlArtifactError.InvalidSections`. `write_zirl_hir_artifact()` in
`src/cache/zirl-hir-artifact.zith` returns `InvalidSections` from two distinct
boundaries without distinguishing them:

1. Its count/`sections`-length preconditions.
2. Any of the five section codecs reporting `failed`.

Facts established so far:

- The fixture's preconditions appear satisfied: `sections` has 5 entries,
  `declaration_count`, `template_count`, and `function_count` are all `0`, and
  each corresponding array is empty.
- `valid_artifact_sections()` in `src/cache/zirl-artifact.zith` requires
  length >= 5, strictly increasing IDs starting at 1, `flags == 1` on the five
  required sections, schema `2` for ID 1 and schema `1` for IDs 2..5, and
  `required_id == 6` at the end. The values written by the typed writer appear
  to satisfy this.
- Therefore the failure is most likely inside one of the section codecs (for
  example Metadata on an empty module) or in the `raw storage->sections[i] =`
  writes, not in the envelope's section validation. This is **not yet
  isolated**.
- Known compiler limitation: reading the writer's internal section array via
  direct raw indexing (`storage.sections[index]`) in the fixture triggers an
  internal compiler error. The fixture must observe section layout through the
  public envelope read path instead of raw indexing.
- Running the aggregate fixture through the VM fails with
  `unsupported slot size in v2 lowering`; `zithc test` only type-checks
  fixtures and does not execute their functions.

Step 6.1's sub-steps (see the canonical plan) are the exact procedure to
isolate and repair this: add structured per-codec `failed` assertions, prove the
envelope accepts a literal five-entry section array via
`write_zirl_artifact()` + `read_zirl_artifact()`, then correct only the boundary
the checks identify. Do not weaken `valid_artifact_sections()` or omit a
required section to make the fixture pass.

The standalone `src/main.zith` check fails because of the experimental
`src/main.zith` / `src/frontend/lexer.zith` files described below; those files
are not part of the cache work and were not rewritten.

Earlier successful harness runs reported deprecated-syntax warnings
(`W1008`, `else if`) from the HIR verifier and Code projection; those warnings
are pre-existing and do not fail the run.

## Worktree inventory (as of this snapshot)

These paths are unrelated to the cache port. They were preserved and committed
separately, not absorbed into the implementation commits. `src/main.zith` and
`src/frontend/lexer.zith` do not pass `zithc check` and are experimental.

| Path | Kind | State |
|---|---|---|
| `CONTEXT.md`, `docs/architecture.md`, `docs/agents/*`, `docs/notes/macos-build-policy.md`, `docs/plans/cross-compile.md`, `docs/plans/roadmap.md`, `docs/reference/full-zith/README.md`, `docs/adr/0001-*`, `docs/adr/0014-*`, `docs/adr/0015-*` | Project-direction documentation retargeting the repository to full Zith | Preserved; see the docs commit |
| `src/frontend/lexer.zith` | Experimental lexer scratch (empty `lexer()` body, keyword-id helper) | WIP; does not compile |
| `src/utils/keywords.zith` | Keyword-enum / keyword-table experiment | WIP; compiles |
| `src/utils/prints.zith` | Colored terminal output helpers used by `main.zith` | WIP; compiles |
| `src/main.zith` | Keyword-`when` experiment driving `prints`/`keywords` | WIP; does not compile |
| `src/cache/cache.zith`, `src/cache/zirl-type-section.zith` | Incidental formatting touches | Preserved as-is |
| `tests/target/*`, `tests/.zith-cache/` | Native test binaries and cache produced by running the harness | Build artifacts; now ignored |

The following work remains:

- **Step 6.1 (blocked):** isolate and repair the typed writer's
  `InvalidSections` failure and make the `zirl-hir-artifact` fixture exit 0
  without weakening the envelope contract or using raw section-array indexing.
- Connect compact section records to the local `HirModule` representation.
  Function identities must use the artifact's canonical compact type table;
  local `HirTypeId` values alone are not a cross-artifact identity, and
  function-signature entries may need to be synthesized.
- **Step 6.2:** reconcile every serialized declaration/template field with a
  lossless HIR source (or an approved contract extension); audit `is_extern`,
  `template_index`, `body_function_index`, `field_name_ids`, `field_type_ids`,
  `method_declaration_indices`, generic-parameter bounds,
  `canonical_field_order`, and `trait_type_ids`.
- **Step 6.3:** build the complete HIR-to-section projection from a verified
  `HirModule` plus explicit artifact identity and paths.
- Add typed section decoding and restore a complete `HirModule` artifact from
  the metadata, declaration, template, attribute, and Code section codecs.
- **Step 6.4:** decode typed sections and hydrate a complete, verified
  `HirModule` artifact, with round-trip and malformed-input fixtures.
- Connect the section projections to `HirModule` and add complete-artifact
  byte-for-byte conformance fixtures. The new writer composes typed payloads,
  but does not yet hydrate a `HirModule`.
- **Step 6.5:** gate derived VIR and codegen on real consumers; keep VIR/codegen
  modules absent while no executable consumer exists, and run the final
  acceptance audit.
- Implement a real Zith frontend adapter only after the local frontend has the
  semantic pipeline needed to produce verified HIR.

- **Step 6.6:** gate frontend interoperability on a real semantic pipeline;
  until a parser, resolver, and proof producer exist, report only the shared
  HIR/cache port and defer the interoperability claim.

## Interoperability Status

Bidirectional cross-frontend cache interoperability is not implemented or
claimed. The local implementation does not yet serialize and restore a
complete `HirModule` artifact (its typed writer is unvalidated and its reader
does not hydrate HIR), and it has no real Zith frontend adapter. The compact
Code section codec alone is not a complete HIR cache reader/writer.
Interoperability requires complete artifacts, real adapters in both compilers,
and conformance tests proving that each can read artifacts produced by the
other.

## Other Plan Documents

`roadmap.md` is a project roadmap sketch, and `cross-compile.md` is a
cross-compilation strategy. They are not records of completed implementation.
