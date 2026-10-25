# Shared HIR Interoperability: Implemented Work

Status snapshot: 2026-10-25 (Step 6.2 producer and port in progress; acceptance audit partial)

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

Historical baseline revalidated on 2026-10-23: `/home/diogo/Zith` was clean
on `main` at `8f313dbf`, 22 commits ahead of `origin/main`. Its ZIRL writer
uses container version 21; the self-Zith port now uses that same version.

## In Progress in `/home/diogo/self-Zith`

The port has completed Step 6.1 and is progressing through the Step 6 cache
contract, projection, and reader work. The following cache foundations and
sections are implemented and committed:

- ZIRL v21 container framing, section validation, and little-endian byte
  primitives.
- Generic artifact-envelope reader/writer for canonical identity,
  dependencies, section tables, payloads, and checksum validation. A typed
  aggregate reader hydrates non-empty Metadata, Decls, Templates, Code, and
  Attrs into caller-owned HIR storage and verifies the completed module.
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
  storage, and serializes Metadata directly from HIR. The non-empty aggregate
  fixture now exercises Decls/Templates projection and serialization.
- The aggregate HIR artifact reader hydrates non-empty Decls/Templates along
  with Metadata, Code, and Attrs into caller-owned storage, reverses canonical
  type IDs, verifies the completed HIR, and rejects failures atomically.
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

## Current Step 6 Status (2026-10-25)

- **Step 6.1 (done):** the typed writer's `InvalidSections` failure was caused
  by imported constants losing their binding semantics; the bounded fix is
  implemented and the `zirl-hir-artifact` fixture exits 0 without weakening
  envelope validation.
- **Step 6.2 (partial):** the HIR contract decisions are approved, and
  self-Zith now has the declaration/template HIR fields, verifier checks,
  schema updates, and HIR-to-Decls/Templates projection. The aggregate fixture
  covers ordered methods and generic parameters, canonical field order,
  distinct Trait references, same-name template identities, linkage facts,
  and atomic invalid-method failures. The named projection matrix now also
  covers insufficient storage for every Decls/Templates projection buffer,
  including Struct field slices. Additional invalid-reference regressions now
  cover a bad function link and name on the second declaration. They exposed a
  verifier validation-expression issue; validating the function link
  separately fixes it. The full local harness passes after these fixes. A
  bounded, uncommitted Zith-- producer/cache tranche is now present in the
  sibling checkout: HIR declarations carry explicit external/body/template
  facts; generic function/struct templates carry bounds, owned signatures or
  fields/order; Trait types survive lowering; and cache projection/hydration
  carries the new records. The generic persistent-cache regression now
  passes. The producer does not yet populate explicit method lists or
  template-trait relations. The only observed method owner is textual
  `ownerName`, and `SymbolData.members` is not populated in this path; do not
  reconstruct either relation by name. No frontend edits are authorized, so
  these producer conformance criteria remain open. The self-Zith projection
  and hydration matrix passes its current fixture coverage, but the
  field-by-field acceptance audit is still required. Details are in
  [`step-6-2-contract-gaps.md`](step-6-2-contract-gaps.md).
- **Step 6.3 (partial):** the typed writer projects Metadata, Decls,
  Templates, Code, and Attrs from verified HIR with caller-owned storage.
  The non-empty aggregate fixture serializes and decodes Decls/Templates
  together with Code/Attrs. The separately named declaration/template
  projection matrix passes in the harness and covers insufficient capacity
  for every Decls/Templates storage buffer. Other malformed/count edge cases
  and the final field-by-field acceptance review remain.
- **Step 6.4 (partial):** the aggregate reader and atomic failure behavior
  hydrate and verify a non-empty Decls/Templates/Code/Attrs artifact, including
  fields, order, methods, traits, and explicit template/declaration identity.
  Round-trip equality and independent insufficient-capacity cases cover every
  hydration buffer. New artifact-level regressions also reject out-of-range
  declaration template/function references and an inconsistent template
  declaration link atomically. Identity-count regressions now independently
  mutate declaration, template, and function counts from the decoded section
  counts and assert `IdentityMismatch` with empty HIR. The reader checks the
  module name and each count separately. The negative Templates-section
  fixture now preserves the original declaration count when replacing only
  Templates; its previous hard-coded count caused an earlier, unrelated
  `IdentityMismatch`. Other malformed/count edge cases and the final
  field-by-field acceptance review remain.
- **Step 6.5 (gate audit passed 2026-10-24):** the source inventory still has
  no executable VIR consumer or codegen backend; the five exact-version gate
  cases are present and called by the test dispatch; ADR-0015 marks both
  consumers `prepared, not implemented`. No placeholder modules were added.
  The overall Step 6 acceptance audit remains open.
- **Step 6.6 (deferred):** add a real Zith frontend adapter only when the
  local frontend has the semantic pipeline needed to produce verified HIR.

- **Latest verification (2026-10-25):** `ZITHC=/home/diogo/Zith/build/zithc
  TMPDIR=/tmp sh tests/run.sh` passes all five checks after the projection
  capacity and artifact identity-count regressions; `git diff --check` is
  clean. The focused artifact fixture also passes in native mode. Running
  that same fixture through `--virtual-machine` still reports
  `unsupported slot size in v2 lowering`; the VM result is a known toolchain
  limitation and is not the harness execution path.
- **Producer verification (2026-10-25):** in the real `/home/diogo/Zith`
  build, `cmake --build ... --target test-cache test-zirl-sections
  test-hir-lower-modern -j2` reported no work, then CTest passed all three
  selected targets. `git diff --check` passes in both repositories. The
  sibling patch remains uncommitted and unstaged; no broader CTest suite was
  run for this tranche.
- The fixture keeps `HirDeclaration.members` empty during artifact round-trip,
  because that general containment slice is not represented in the current
  Decls record. A separate projection check confirms non-empty `members` do
  not replace the explicit ordered method list. Whether general containment
  itself must be persisted remains an unresolved contract question; no schema
  change is included here.
- The artifact-reader fixture rewrites the section checksum after mutating
  persisted links, so the new invalid-reference cases exercise typed
  validation rather than stopping at envelope checksum rejection.

## Interoperability Status

Bidirectional cross-frontend cache interoperability is not implemented or
claimed. The local writer projects all five typed sections, and the aggregate
reader now handles the non-empty Decls/Templates fixture, but the full
projection/hydration conformance matrix remains incomplete. There is no real
Zith frontend adapter. Interoperability requires complete artifacts, real
adapters in both compilers, and conformance tests proving that each can read
artifacts produced by the other.

## Other Plan Documents

`roadmap.md` is a project roadmap sketch, and `cross-compile.md` is a
cross-compilation strategy. They are not records of completed implementation.
