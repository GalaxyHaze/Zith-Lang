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
`8c12c9bd`, 21 commits ahead of `origin/main`.

## In Progress in `/home/diogo/self-Zith`

The port is in Step 6 of the plan. The following cache foundations and
sections are implemented:

- ZIRL v22 container framing, section validation, and little-endian byte
  primitives.
- Compact HIR type-entry encoding.
- Metadata schema 2, including strings, paths, types, and nominal definitions.
- Decls, Templates, and Attrs section codecs.
- Compact Code section codec for expressions, functions/blocks, globals,
  vtables, and canonical mappings, using caller-owned decode storage. Function
  records preserve `foreign_adapter_name_id`, and Call expressions preserve
  the variadic-slice fields `variadic_slice_param`, `is_variadic_slice`, and
  `auto_collect_tail`. Container version bumps reject incompatible older
  layouts.
- Caller-owned decode storage and failure handling for the section codecs;
  tests cover golden bytes, round-trips, truncation, invalid values, and
  short output buffers in the implemented sections.

The cache-port baseline before the Code section work was `800a60a`
(`feat(cache): add ZIRL attributes section codec`). The Code section codec and
its golden-byte, round-trip/reserialization, truncation, insufficient-capacity,
short-writer, and malformed-vtable tests pass with the sibling build's
compiler:

```sh
ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh tests/run.sh
```

The latest test run passed. It reports deprecated-syntax warnings from the HIR
verifier. The standalone `src/main.zith` check remains blocked by the
pre-existing undefined `getKeywordId` reference; that change was not part of
the cache work.

The following work remains:

- Connect compact section records to the local `HirModule` representation.
  Function identities must use the artifact's canonical compact type table;
  local `HirTypeId` values alone are not a cross-artifact identity, and
  function-signature entries may need to be synthesized.
- Add full-artifact reader/writer orchestration and automated complete-artifact
  byte-for-byte conformance fixtures.
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
