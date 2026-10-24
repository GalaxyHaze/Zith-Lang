# Shared HIR Interoperability: Implemented Work

Status snapshot: 2026-10-24 (Step 6.2 projection in progress; full hydration partial)

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

The port has completed Step 6.1 and is progressing through the Step 6 cache
contract, projection, and reader work. The following cache foundations and
sections are implemented and committed:

- ZIRL v21 container framing, section validation, and little-endian byte
  primitives.
- Generic artifact-envelope reader/writer for canonical identity,
  dependencies, section tables, payloads, and checksum validation. A typed
  aggregate reader now hydrates the currently supported HIR subset into
  caller-owned storage; non-empty Decls/Templates remain explicitly rejected.
- Compact HIR type-entry encoding.
- Metadata schema 3, including strings, paths, types, nominal definitions, and
  the shared Trait type kind.
- Decls schema 2 preserves declaration-level `discardable` in its former
  reserved byte; malformed booleans and schema-1 payloads are rejected.
- Templates and Attrs section codecs.
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
- HIR-to-Attrs projection: `build_zirl_hir_attrs_section()` verifies the HIR,
  preflights caller-owned capacities, preserves source slot/expression/
  function indices, flattens call escapes, omits entries with no residual
  facts, and persists every safety proof including `present == false`.
  Capacity and invalid-HIR failures return empty section slices. The focused
  fixture and full self-Zith harness pass.
- Typed section-to-container writer: Metadata, Decls, Templates, Code, and
  Attrs payloads are encoded in the required ZIRL order with fixed section IDs
  and schema versions, then passed to the validated artifact-envelope writer.
  This slice is committed (`9d56fe2`) and is exercised end to end by the
  self-Zith harness after the Step 6.1 fix below.
- The typed HIR artifact writer now accepts identity, paths, and `HirModule`
  rather than caller-prebuilt section records. It verifies HIR, projects
  canonical types, Decls, Templates, Code, and Attrs using caller-owned
  storage, and serializes Metadata directly from HIR. Focused tests for the
  new Decls/Templates projections and non-empty aggregate output remain
  pending.
- The HIR verifier now accepts `HirRuntimePanic` expressions, which were
  previously rejected by the fallback expression-shape branch despite being
  part of the HIR/Code contract. A focused verifier regression covers it.
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

## Resolved: Step 6.1 (imported constant loses binding semantics)

The baseline harness run on 2026-10-23 with
`ZITHC=/home/diogo/Zith/build/zithc` exited 2. Isolating its steps showed that
five of the six passed and only the final fixture failed:

| Step | Result |
|---|---|
| `zithc --include <tmp>/src test <tmp>` | 0 |
| `run --virtual-machine --no-cache tests/run.zith` | 0 |
| `run --no-cache tests/canonical-types.zith` | 0 |
| `run --no-cache tests/hir-contract.zith` | 0 |
| `run --no-cache tests/zirl-artifact.zith` | 0 |
| `run --no-cache tests/zirl-hir-artifact.zith` | **2** |

The root cause is upstream of the cache codecs: `LocalSymbolInfo` does not
preserve `BindingKind`, and the import-resolution paths do not copy it into
`ResolvedName`. The default binding classification makes HIR lowering treat
an imported constant as a local binding instead of producing
`HirGlobalConstLoad`; this causes the typed writer's `InvalidSections` result.
There is a related semantic bug: `isConstantExpression()` looks up a
provider-module declaration ID in the consumer module's snapshot.

The fix and its regression tests are implemented in `/home/diogo/Zith`
(uncommitted working-tree changes; no staging, commit, or push). The bounded
implementation is recorded in
[`step-6-1-imported-const-fix.md`](step-6-1-imported-const-fix.md). It:

- adds `frontend::BindingKind bindingKind` to `LocalSymbolInfo` and copies it
  when building module symbols;
- copies `symbol.bindingKind` into `ResolvedName` in all four import-copy
  paths in `frontend-symbol-resolution.cpp` (selector import, `from`/`export`
  injection, qualified struct literal, qualified field chain);
- classifies imported constants in `PerModuleSema::isConstantExpression()`
  from `resolved->declKind`/`resolved->bindingKind` instead of indexing the
  consumer snapshot with the provider's symbol id;
- adds the missing `HirGlobalConstLoad` path for `ns.CONST` in
  `lowerField()` in `hir-lower-expr-agg.cpp`, which previously returned
  `kInvalidHirExpr` because the import branch only handled functions.

Validation on 2026-10-24:

| Check | Result |
|---|---|
| Three new regressions in `test-hir-lower-modern` | pass (selective, `from`, qualified) |
| Focused CTest (`test-frontend-context`, `test-frontend-modern-pipeline`, `test-hir-lower-modern`, `test-codegen`) | pass except the pre-existing unrelated `test_generic_template_survives_persistent_cache` failure |
| `ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh tests/run.sh` | exit 0; `zirl-hir-artifact` no longer returns `InvalidSections` |
| Full CTest suite | 43/44 pass; only the pre-existing unrelated `test-hir-lower-modern` cache test fails |

The envelope validation contract was not weakened and no ZIRL section was
omitted.

The silent codegen behavior that omits stores when `emitVar` returns
`nullptr` is an independent follow-up, registered as its own issue and
outside this fix's implementation scope.

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

- **Step 6.1 (done):** the typed writer's `InvalidSections` failure was caused
  by imported constants losing their binding semantics; the bounded fix is
  implemented and the `zirl-hir-artifact` fixture exits 0 without weakening
  envelope validation.
- **Step 6.2 (partial):** the HIR contract decisions are approved, and
  self-Zith now has the declaration/template HIR fields, verifier checks,
  schema updates, and HIR-to-Decls/Templates projection. The full per-relation
  positive/negative fixture matrix remains pending, as do producer population
  in `/home/diogo/Zith` and aggregate-reader hydration. Details and acceptance
  criteria are in [`step-6-2-contract-gaps.md`](step-6-2-contract-gaps.md).
- **Step 6.3 (partial):** the typed writer projects Metadata, Decls,
  Templates, Code, and Attrs from verified HIR with caller-owned storage.
  Existing empty-artifact and Code/Attrs non-empty fixtures pass; dedicated
  non-empty Decls/Templates projection and aggregate serialization fixtures
  remain pending.
- **Step 6.4 (partial):** the aggregate reader and atomic failure behavior
  hydrate and verify the representable Metadata/Code/Attrs subset. Complete
  artifact hydration depends on resolving the Step 6.2 Decls/Templates gaps.
- **Step 6.5 (gate audit passed 2026-10-24):** the source inventory still has
  no executable VIR consumer or codegen backend; the five exact-version gate
  cases are present and called by the test dispatch; ADR-0015 marks both
  consumers `prepared, not implemented`. No placeholder modules were added.
  The overall Step 6 acceptance audit remains open.
- **Step 6.6 (deferred):** add a real Zith frontend adapter only when the
  local frontend has the semantic pipeline needed to produce verified HIR.

## Step 6 Progress (2026-10-24)

- Attrs now has an independent schema version 2. Its proof records preserve
  `present` as a canonical trailing boolean byte; schema 1 is rejected, and
  the container version remains 21 because the envelope did not change.
- Declaration-level `discardable` now round-trips as a canonical `0/1`
  boolean in Decls schema 2. The HIR schema is unchanged because the source
  field already exists on `HirDeclaration`.
- Regression fixtures cover true/false proof round-trips, truncated and
  invalid boolean bytes, and prove that an absent proof does not remove a
  `RequiredCheck`. VIR reuse tests cover exact-match, mismatches, and zero
  versions.
- Step 6.3 remains partial: the HIR artifact source now contains only identity,
  paths, and `HirModule`; the writer verifies HIR, builds the canonical type
  table in caller-owned storage, projects Decls/Templates/Code/Attrs, and
  serializes Metadata directly from HIR. The non-empty
  fixture exercises strings, Int/Ptr/Array types, a function with a parameter
  and block, a global, a vtable, slot attrs, and present/absent safety proofs.
  It checks projection and serialization, section IDs/schemas, Code/Attrs
  decoding, source-slice independence, and deterministic bytes across repeated
  writes. Dedicated positive/negative projection fixtures and non-empty
  aggregate serialization are not yet present.
- Step 6.2's approved HIR relationships and semantic fields are implemented
  through projection in self-Zith; the upstream Zith-- producer and the
  focused projection fixture matrix remain outstanding.
- Step 6.4 is partial. The aggregate reader validates the envelope and typed
  sections, hydrates Metadata/Code/Attrs into caller-owned HIR storage,
  reverses canonical type IDs, and runs `verify_hir()` before returning.
  Field-by-field comparison covers every `HirModule` field; identity and paths
  are compared separately, including dependency ABI fields. The non-empty
  round-trip exercises a global, vtable, slot attrs, call attrs, function
  attrs, and present/absent proofs. Negative cases cover malformed artifacts
  and insufficient capacity across the populated hydration-storage families;
  every tested failure asserts empty public HIR slices. Empty artifacts now
  round-trip through the public writer and aggregate reader and pass
  `verify_hir()`. Separate valid-envelope fixtures encode one non-empty Decls
  record and one non-empty Templates record through their public section
  writers; each reaches the aggregate reader's
  `UnsupportedDeclarationsOrTemplates` rejection and exposes empty public HIR
  slices. The focused fixture and full harness pass. This is not
  complete-artifact hydration: non-empty Decls/Templates cannot be projected
  or restored until their HIR contract sources are resolved and approved
  under Step 6.2.
- Step 6.5's five VIR version-gate cases and ADR-0015 readiness requirements
  were revalidated: the full harness passes and the test dispatch executes the
  five-case gate. They establish exact-match reuse policy only; no VIR
  consumer, generator, or codegen backend is implemented, and the final Step
  6 acceptance audit remains pending. The whole-worktree `git diff --check`
  still reports pre-existing trailing whitespace at
  `src/cache/zirl-buffer.zith:85`; the standalone `src/main.zith` check remains
  blocked by unresolved names in the experimental entry point.
- Step 6.6 remains **DEFERRED** because the frontend inventory contains only
  the experimental lexer.

## Interoperability Status

Bidirectional cross-frontend cache interoperability is not implemented or
claimed. The local writer now projects the five typed sections from shared HIR,
but dedicated non-empty Decls/Templates fixtures and aggregate-reader
hydration remain outstanding; the reader still rejects non-empty
Decls/Templates. There is no real Zith frontend adapter. Interoperability
requires complete artifacts, real adapters in both compilers, and conformance
tests proving that each can read artifacts produced by the other.

## Other Plan Documents

`roadmap.md` is a project roadmap sketch, and `cross-compile.md` is a
cross-compilation strategy. They are not records of completed implementation.
