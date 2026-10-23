# Shared HIR Interoperability: Implemented Work

Status snapshot: 2026-10-23

This document records implementation reported for the plan
`shared-hir-interoperability.md`. It is a progress summary, not a replacement
for the canonical plan.

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

The port is in Step 6 of the plan. The following cache foundations and
sections are implemented:

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
  The empty-HIR fixture calls the writer, but currently receives
  `InvalidSections`; success-path layout assertions and the undersized-buffer
  failure test remain incomplete. Envelope reading remains covered by the
  separate container test.
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

The full harness passed on 2026-10-23 before the typed section-to-container
writer fixture was added. The current fixture passes `zithc check`, but its
native run initially hit an internal compiler error when the fixture directly
read fields through raw section-array indexing. Removing those reads lets the
function compile, but the writer returns `ZirlArtifactError.InvalidSections`
(exit code 2). Running the aggregate fixture through the VM fails with
`unsupported slot size in v2 lowering`. Therefore the current full harness
does not pass, and this writer slice is not validated. Earlier successful
harness runs reported deprecated-syntax warnings from the HIR verifier. The standalone
`src/main.zith` check currently fails on syntax errors in the uncommitted
`src/main.zith` and `src/utils/keywords.zith` worktree changes; those files
were not part of the cache work.

The following work remains:

- Connect compact section records to the local `HirModule` representation.
  Function identities must use the artifact's canonical compact type table;
  local `HirTypeId` values alone are not a cross-artifact identity, and
  function-signature entries may need to be synthesized.
- Add typed section decoding and restore a complete `HirModule` artifact from
  the metadata, declaration, template, attribute, and Code section codecs.
- Connect the section projections to `HirModule` and add complete-artifact
  byte-for-byte conformance fixtures. The new writer composes typed payloads,
  but does not yet hydrate a `HirModule`.
- Finish the shared VIR/codegen port and run the final acceptance audit.
- Implement a real Zith frontend adapter only after the local frontend has the
  semantic pipeline needed to produce verified HIR.

The `self-Zith` worktree contains unrelated modifications and untracked files.
They have been preserved and excluded from the implementation commits.

## Interoperability Status

Bidirectional cross-frontend cache interoperability is not implemented or
claimed. The local implementation does not yet serialize and restore a
complete `HirModule` artifact, and it has no real Zith frontend adapter. The
compact Code section codec alone is not a complete HIR cache reader/writer.
Interoperability requires complete artifacts, real adapters in both compilers,
and conformance tests proving that each can read artifacts produced by the
other.

## Other Plan Documents

`roadmap.md` is a project roadmap sketch, and `cross-compile.md` is a
cross-compilation strategy. They are not records of completed implementation.
