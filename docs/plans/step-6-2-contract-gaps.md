# Step 6.2: Declaration and Template HIR Contract

**Objective:** make every Decls and Templates record field available through
verified, frontend-neutral HIR and preserve it losslessly in the self-Zith
artifact projection and reader.

**Status:** Contract decisions accepted on 2026-10-24. The self-Zith HIR
contract, verifier checks, schema updates, Decls/Templates projection, and
non-empty aggregate-reader hydration are implemented. The separately named
positive/negative projection matrix is implemented and called by the artifact
fixture; each caller-owned Decls/Templates projection buffer now has an
insufficient-capacity regression. On 2026-10-25, a bounded producer/cache
patch was applied in `/home/diogo/Zith`: declarations now carry explicit
external/body/template facts, generic declarations produce template records
with generic bounds, owned signatures or fields/order, Trait types are
preserved, and cache projection/hydration handles those records. The focused
CTest targets `test-cache`, `test-zirl-sections`, and
`test-hir-lower-modern` pass (3/3), and the self-Zith harness passes (5/5).
The producer acceptance audit remains partial: method lists and template
trait relations are not populated because the available owner field is
textual (`ownerName`) and this path does not populate `SymbolData.members`.
No relation may be inferred from names, and frontend changes are outside the
authorization. Keep these relations and their conformance cases open.

**Preconditions:** The user authorized bounded changes in `/home/diogo/Zith`
for this plan, excluding frontend edits, large structural redesigns, and the
separate codegen follow-up. The sibling is on `main`, one commit ahead of
`origin/main`, with an uncommitted 18-file producer/cache patch. Before
continuing, run `git -C /home/diogo/Zith status --short --branch` and
`git -C /home/diogo/Zith diff --name-only`; compare the output against the
current status record and preserve all changes. If files or hunks are
unexpected, stop and report them. Obtain sandbox approval separately for
build/test commands and writes under `/home/diogo/Zith`. Do not stage or
commit.

**Approved contract:**

- `HirDeclaration.is_extern` is the declaration-owned linkage fact. Never
  derive it from `HirFunction.foreign_c` or `has_body`.
- `HirDeclaration.body_function_id` is an optional `HirFunctionId`, encoded
  in HIR as `invalidFunction` when absent and in Decls as
  `0xFFFFFFFFu32`. A function name is never a relation.
- `HirDeclaration.template_id` is an optional `HirTemplateId`, encoded in HIR
  as `invalidTemplate` when absent and in Decls as `0xFFFFFFFFu32`.
- `HirModule.templates` owns the HIR template table. Every template requires
  a declaration ID; the declaration's `template_id` and the template's
  `declaration_id` must agree.
- `HirTemplate` owns generic parameters/bounds, its signature, external bit,
  fields, canonical field order, methods, and trait references. Its
  `function_id` is an optional explicit relation only; the owned signature is
  authoritative and must not be reconstructed from names.
- `HirDeclaration.method_declaration_ids` and
  `HirTemplate.method_declaration_indices` are explicit ordered method lists.
  `HirDeclaration.members` remains general containment and is not used as a
  method list.
- `HirTypeKind.Trait` represents a trait type. Its `ref0` is a valid
  `HirStringId`; it must not collapse to `Unknown`. Template trait references
  and generic bounds use HIR type IDs and are mapped through the canonical
  artifact type table.
- Bump the independent HIR schema to 2. Bump Metadata section schema to 3
  because its type-kind domain gains `Trait`. Keep Decls schema 2 and Code
  schema 1. Bump Templates from schema 1 to schema 2 because every template
  record must persist its explicit `declaration_id` and optional
  `function_id`; neither may be reconstructed by inference. Keep Attrs schema
  2 and container version 21.
- Complete Decls/Templates projection and hydration atomically. Any invalid
  reference, inconsistent relationship, unsupported mapping, or insufficient
  storage returns an error and empty public slices; never publish partial
  sections or a partially hydrated module.

## Implementation Steps

### 1. Extend the Zith-- producer contract

**Goal:** `/home/diogo/Zith` emits every accepted declaration/template fact
that can be sourced without frontend changes or name-based inference, and
tests all emitted facts.

**Sub-steps:**
1. Audit the actual dirty patch. Run:

   ```sh
   git -C /home/diogo/Zith status --short --branch
   git -C /home/diogo/Zith diff --name-only
   git -C /home/diogo/Zith diff --check
   ```

   Expected: branch `main`, one commit ahead of `origin/main`, and exactly
   these 18 modified paths: `src/cache/artifact-builder.cpp`,
   `src/cache/cache-identity.hpp`, `src/cache/cache-types.hpp`,
   `src/hir/hir-module.cpp`, `src/hir/hir-module.hpp`,
   `src/sema/hir-lower-modern.cpp`, `src/sema/hir-lower-types.cpp`,
   `src/session/compilation-session.cpp`,
   `src/session/persistent-cache.cpp`, `src/symbols/symbol-table.cpp`,
   `src/symbols/symbol-table.hpp`, `src/zirl/zirl-decl-section.cpp`,
   `src/zirl/zirl-header.hpp`, `src/zirl/zirl-reader.cpp`,
   `src/zirl/zirl-writer.cpp`, `tests/test-cache.cpp`,
   `tests/test-hir-lower-modern.cpp`, and `tests/test-zirl-sections.cpp`.
   If the branch, path set, or purpose differs, stop and report it. Do not
   stage, commit, stash, reset, clean, or overwrite any path.
2. Inspect the existing producer changes in
   `/home/diogo/Zith/src/hir/hir-module.hpp`,
   `/home/diogo/Zith/src/hir/hir-module.cpp`, and
   `/home/diogo/Zith/src/sema/hir-lower-modern.cpp`. Confirm explicit
   `is_extern`, optional `body_function`, reciprocal declaration/template
   IDs, generic parameter/bound order, owned function signatures, struct
   fields/canonical order, and the invalid-function sentinel when no HIR
   function represents the generic declaration. Do not recreate already
   present fields.
3. Inspect
   `/home/diogo/Zith/src/sema/hir-lower-types.cpp`,
   `/home/diogo/Zith/src/cache/artifact-builder.cpp`,
   `/home/diogo/Zith/src/session/persistent-cache.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-decl-section.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-reader.cpp`, and
   `/home/diogo/Zith/src/zirl/zirl-writer.cpp`. Confirm Trait type identity,
   template signature flags, explicit declaration/function indices,
   independent schema versions, bounds checks before indexed access, and
   atomic hydration behavior.
4. Inspect the exact relation sources in
   `/home/diogo/Zith/src/frontend/frontend.hpp`,
   `/home/diogo/Zith/src/symbols/symbol-table.hpp`, and
   `/home/diogo/Zith/src/sema/hir-lower-modern.cpp`. The current producer
   exposes textual `ownerName`, and `SymbolData.members` is not populated for
   this path. If no non-frontend semantic owner/type ID already exists, stop
   without filling method or template-trait relations. Do not edit
   `src/frontend/*`, compare declaration names, or use general `members` as
   an ordered method list.
5. Add conformance tests only for available stable facts in
   `/home/diogo/Zith/tests/test-hir-lower-modern.cpp`. Assert exact relation
   IDs, values, and ordering; add malformed-link and signature-mismatch
   tests only where a linked function is actually emitted. Keep method and
   template-trait producer tests explicitly pending when their owner identity
   is unavailable.
6. Request sandbox approval, then build the exact focused targets:

   ```sh
   cmake --build /home/diogo/Zith/build --target \
     test-cache test-zirl-sections test-hir-lower-modern -j2
   ```

7. Request sandbox approval separately, then run:

   ```sh
   ctest --test-dir /home/diogo/Zith/build \
     -R '^(test-cache|test-zirl-sections|test-hir-lower-modern)$' \
     --output-on-failure
   ```

**Expected output:** build exits 0 and CTest reports all three named targets
passed. `git diff --check` exits 0. The producer tests cover every emitted
fact; unavailable method/trait ownership remains an explicit open acceptance
item rather than a fabricated relation.

**Failure checks:**
- If the live dirty path set or diff differs from the recorded patch, stop and
  report the exact output before editing.
- If any required relation can only be recovered by `ownerName`, a declaration
  name, or general symbol membership, stop that relation and leave its test
  pending. Do not change frontend files under this plan.
- If a generic bound cannot be lowered to a representable HIR type, preserve
  the failure as an explicit producer error and do not emit an empty bound
  list.
- If the linked function's signature differs from the template-owned
  signature, return a producer/test failure; do not pick one silently.
- If any bounds check happens after indexing a type/function/declaration
  table, correct the ordering and add an invalid-index regression before
  accepting the producer patch.

**Success criteria:** the three focused CTest targets pass; every populated
field is asserted using exact IDs and values; `git diff --check` passes; and
all unavailable owner-based relations remain explicitly open without
front-end changes or name-based inference.

### 2. Verify the HIR contract and version it

**Goal:** self-Zith rejects malformed declaration/template links and
incomplete template tables before projection or consumption.

**Sub-steps:**
1. In `/home/diogo/self-Zith/src/ir/hir.zith`, finish the accepted fields and
   add `HirTypeKind.Trait` with `ref0` documented as a string ID.
2. In `/home/diogo/self-Zith/src/ir/hir-verify.zith`, validate optional
   function/template sentinels, required template declaration IDs, reciprocal
   declaration/template ownership, enum ranges, all names/types, equal
   field-name/type lengths, equal function parameter type/name lengths,
   canonical field-order permutation, method declaration references, trait
   type kinds, and generic bound references. Append new error enum values;
   do not renumber existing values.
3. Keep absent optional links valid only when equal to their exact invalid-ID
   sentinel. Reject other out-of-range values.
4. In `/home/diogo/self-Zith/src/cache/cache.zith` and
   `/home/diogo/self-Zith/src/cache/zirl-container.zith`, set the HIR schema
   to 2. Set Metadata schema to 3 in the container schema map. Leave container
   version 21 and the other section schemas unchanged.
5. Update every `HirModule` literal in
   `/home/diogo/self-Zith/tests/hir-contract.zith`,
   `/home/diogo/self-Zith/tests/canonical-types.zith`,
   `/home/diogo/self-Zith/tests/zirl-hir-artifact.zith`, and
   `/home/diogo/self-Zith/src/cache/zirl-hir-artifact.zith` to initialize the
   template slice.
6. Add separate verifier fixtures for a valid declaration/template
   relationship, invalid function/template IDs, broken reciprocal ownership,
   mismatched field arrays, invalid field order, invalid method references,
   and a non-Trait type in a trait-only list.
7. Run:

   ```sh
   ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh /home/diogo/self-Zith/tests/run.sh
   ```

**Expected output:** all five harness programs pass and all malformed cases
return the intended verifier error at the owning declaration/template index.

**Failure checks:**
- If a valid producer fixture fails, compare its exact IDs/lengths against
  the contract before relaxing validation.
- If an invalid fixture passes, add the missing invariant to the verifier;
  do not move validation into the cache codec only.
- If the compiler reports an uninitialized `HirModule` field, locate every
  literal with `rg -n 'HirModule \{' /home/diogo/self-Zith/src
  /home/diogo/self-Zith/tests` and initialize the actual owner explicitly.

**Success criteria:** the full harness exits 0 and each new positive and
negative verifier fixture is called by `tests/hir-contract.zith` dispatch.

### 3. Project complete Decls and Templates sections

**Goal:** a verified HIR module projects every Decls/Templates field to
caller-owned section storage with canonical type IDs, explicit template links,
and no inferred values.

**Sub-steps:**
1. Open `/home/diogo/self-Zith/src/cache/zirl-hir-artifact.zith`. Add
   `ZirlDeclStorage` and `ZirlTemplateStorage` fields to
   `ZirlHirArtifactWriteStorage`. Do not allocate memory in projection code;
   all records and flattened nested arrays must be caller-owned.
2. Extend `ZirlHirArtifactSections` and `ZirlHirArtifactProjectionResult` in
   that file to carry `ZirlDeclSection` and `ZirlTemplateSection`. Update
   `failed_hir_artifact_projection()` so every error returns empty public
   slices for Code, Attrs, Decls, and Templates.
3. Add a declaration projection helper in
   `/home/diogo/self-Zith/src/cache/zirl-hir-artifact.zith`. Copy kind,
   visibility, `is_extern`, `discardable`, module depth, name ID, and ordered
   method declaration IDs from each `HirDeclaration`. Map declaration
   `type_id` through `ZirlCanonicalTypes.local_type_ids`. Preserve optional
   `template_id` and `body_function_id` exactly, encoding their absent value
   as `0xFFFFFFFFu32`.
4. In the declaration helper, when `kind` is Struct, require `type_id` to
   select a valid HIR Struct type and its `ref0` to select an existing
   `HirStructDef`. Copy field names in source order and map each field type
   through `local_type_ids`. Emit empty declaration field arrays for all
   other declaration kinds. Never match a type, method, function, or template
   by name.
5. Add a template projection helper in the same file. Copy
   `declaration_id`, `function_id`, kind, `is_extern`, name ID, return type,
   generic parameters and bounds, parameter types/names, field names/types,
   canonical field order, ordered method declaration IDs, and trait type IDs.
   Preserve string, declaration, and function IDs; map every type ID through
   `local_type_ids`. Encode only an absent `function_id` as `0xFFFFFFFFu32`.
6. If `function_id` is present, compare the linked `HirFunction` against the
   template-owned return type, parameter types and names, variadic/state/
   foreign-C/tail-calling-convention flags. Reject any mismatch. Do not
   populate template-owned signature facts from the linked function. Persist
   those four booleans in Templates schema 2 by assigning bits 0 through 3 of
   the first former reserved prefix byte (variadic, state, foreign-C, and
   tail-calling-convention); require the second prefix byte and all unknown
   flag bits to be zero.
7. Before writing nested slices, count declarations, templates, struct fields,
   declaration methods, generic parameters, generic bounds, template
   parameters, template fields, canonical-order entries, template methods,
   and trait references. Check `u32` overflow and every corresponding
   caller-owned capacity. On any failure, return empty public sections and a
   structured projection/capacity error.
8. Reject the projection unless `identity.declaration_count`,
   `identity.template_count`, and `identity.function_count` equal the
   verified HIR table counts. Return `InvalidIdentity` before serialization.
9. Update `project_zirl_hir_artifact_sections()` and
   `write_zirl_hir_artifact()` to serialize the projected Decls/Templates
   sections instead of stack-local empty sections. On any section-writer
   failure, do not publish any section payload slice.
10. In `/home/diogo/self-Zith/src/cache/zirl-template-section.zith`, add
    `declaration_index` and `function_index` to `ZirlTemplateRecord`.
    Serialize and decode these two `u32` values immediately after `name_id`;
    retain `0xFFFFFFFFu32` for an absent function index.
11. Add `zirlTemplatesSectionSchemaVersion = 2u16` in
    `/home/diogo/self-Zith/src/cache/zirl-container.zith`. Require schema 2
    for section ID 3 in both `validate_zirl_sections()` and
    `valid_artifact_sections()` in
    `/home/diogo/self-Zith/src/cache/zirl-artifact.zith`. Write schema 2 from
    `write_zirl_hir_artifact()`. Change all section-3 schema assertions in
    `/home/diogo/self-Zith/tests/zirl-hir-artifact.zith` and
    `/home/diogo/self-Zith/tests/zirl-artifact.zith` to the new constant.
12. Add every fixture named in the projection fixture matrix below to
    `/home/diogo/self-Zith/tests/zirl-hir-artifact.zith` and call each from its
    test dispatch. Keep the verifier fixtures in
    `/home/diogo/self-Zith/tests/hir-contract.zith`. The aggregate writer
    fixture must check non-empty declarations/templates, exact
    declaration/function IDs, and section 3 schema 2.
13. Run:

   ```sh
   ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh /home/diogo/self-Zith/tests/run.sh
   git -C /home/diogo/self-Zith diff --check -- src/ir/hir.zith src/ir/hir-verify.zith src/cache/cache.zith src/cache/zirl-container.zith src/cache/zirl-artifact.zith src/cache/zirl-type-section.zith src/cache/zirl-canonical-types.zith src/cache/zirl-metadata.zith src/cache/zirl-decl-section.zith src/cache/zirl-template-section.zith src/cache/zirl-hir-artifact.zith tests/hir-contract.zith tests/canonical-types.zith tests/zirl-artifact.zith tests/zirl-hir-artifact.zith
   ```

**Expected output:** all positive fixtures preserve exact values/order and all
negative fixtures return a non-None error with empty Decls/Templates slices.

**Failure checks:**
- If any field has no exact producer source, return an explicit error and
  stop; do not write a partial record or use a sentinel for unknown data.
- If canonical type mapping fails, return the canonical-type error before
  exposing either section.
- If short storage is supplied for any nested slice, return the capacity
  error and empty both section outputs.
- If section counts differ from identity counts, reject before serializing.

**Success criteria:** projection fixtures prove every serialized Decls and
Templates field, explicit declaration/function IDs and Templates schema 2,
plus each negative/capacity/identity failure path, and the full harness exits
0.

### 4. Hydrate and round-trip declarations/templates

**Goal:** the aggregate HIR reader reconstructs every declaration/template
field and explicit link, verifies the complete module, and returns no partial
module on failure.

**Sub-steps:**
1. Extend `ZirlHirArtifactHydrationStorage` in
   `/home/diogo/self-Zith/src/cache/zirl-hir-artifact.zith` with caller-owned
   HIR buffers for declarations, templates, generic parameters, generic
   bounds, template parameter types/names, template fields, canonical field
   order, methods, and trait types.
2. Make `read_zirl_hir_artifact_sections()` accept Templates schema 2 only
   through the container schema validator, decode both explicit template
   indices, and reject out-of-range declaration/function IDs before exposing
   the section.
3. In `read_zirl_hir_artifact_module()`, pre-count decoded nested values with
   checked `u64` additions. Check every output capacity before constructing
   any nested HIR slice.
4. Hydrate every declaration/template field. Reverse each canonical type ID
   through the inverse local/canonical mapping. Preserve string IDs,
   declaration IDs, function IDs, order, and exact invalid sentinels.
5. Set `HirTemplate.declaration_id` and `function_id` from the explicit
   Templates schema-2 fields. Do not infer either ID from names, declaration
   ownership, or Code functions.
6. Call `verify_hir()` only after every HIR table is populated. On decode,
   capacity, type-map, identity, relationship, or verification failure, return
   the existing empty failure module and empty public declaration/template
   slices.
7. Add aggregate round-trip assertions for every declaration/template field,
   both explicit IDs, identity, paths, and ordered arrays. Mutate source HIR
   nested arrays after writing and verify that serialized bytes and decoded
   values remain unchanged.
8. Add negative reader fixtures for invalid explicit IDs, signature mismatch,
   malformed Templates schema-2 payloads, and insufficient capacity in every
   newly added storage family. Assert each failure exposes only empty public
   slices.
9. Run:

   ```sh
   ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh /home/diogo/self-Zith/tests/run.sh
   git -C /home/diogo/self-Zith diff --check -- src/ir/hir.zith src/ir/hir-verify.zith src/cache/cache.zith src/cache/zirl-container.zith src/cache/zirl-artifact.zith src/cache/zirl-type-section.zith src/cache/zirl-canonical-types.zith src/cache/zirl-metadata.zith src/cache/zirl-decl-section.zith src/cache/zirl-template-section.zith src/cache/zirl-hir-artifact.zith tests/hir-contract.zith tests/canonical-types.zith tests/zirl-artifact.zith tests/zirl-hir-artifact.zith
   ```

**Expected output:** valid empty and non-empty artifacts round-trip exactly;
each malformed or undersized artifact returns an error and an empty public
module.

**Failure checks:**
- If local/canonical type IDs are not inverse mappings, stop at the first
  differing field and correct the mapping table.
- If a failed read exposes a non-empty declaration/template slice, fix the
  failure-result constructor before adding more fixtures.
- If any array aliases source HIR or decode storage unexpectedly, preserve
  the first failing index and correct the ownership layout.

**Success criteria:** field-by-field round-trip passes for all module tables,
and every failure-path fixture proves atomic empty output.

## Fixture Matrix

Add these separately named projection test functions in
`/home/diogo/self-Zith/tests/zirl-hir-artifact.zith`; call every function from
its test dispatch:

| Test function | Required assertions |
|---|---|
| `test_hir_decl_projection_preserves_extern` | True and false values survive despite conflicting function facts; declaration value is authoritative. |
| `test_hir_decl_projection_preserves_body_link` | A nonzero `HirFunctionId` survives exactly; invalid/out-of-range links reject without name matching. |
| `test_hir_decl_projection_preserves_template_membership` | A nonzero `HirTemplateId` survives exactly; invalid/out-of-range links reject. |
| `test_hir_decl_projection_preserves_method_indices` | Two explicit ordered declaration method IDs survive; invalid or non-function method references reject; general `members` are ignored. |
| `test_hir_template_projection_preserves_identity` | Two same-name templates remain distinct and declaration membership selects by ID. |
| `test_hir_template_projection_preserves_generic_parameters` | Two ordered parameters and their ordered bound types survive; invalid references reject. |
| `test_hir_template_projection_preserves_function_signature` | Owned return/parameter types, names, and external state survive; a mismatched linked function rejects. |
| `test_hir_template_projection_preserves_fields` | Two ordered fields survive with canonical type IDs; count mismatch and invalid type reject. |
| `test_hir_template_projection_preserves_field_order` | A non-identity permutation such as `[1, 0]` survives; duplicate/missing/out-of-range entries reject. |
| `test_hir_template_projection_preserves_methods` | Two ordered template method IDs survive; invalid references reject. |
| `test_hir_template_projection_preserves_traits` | Two Trait-kind type IDs survive canonically; non-Trait/invalid types reject. |

## Final Acceptance

Run the upstream focused test only after obtaining command approval, then run
the full self-Zith harness and path-scoped whitespace check:

```sh
cmake --build /home/diogo/Zith/build --target test-hir-lower-modern
ctest --test-dir /home/diogo/Zith/build -R '^test-hir-lower-modern$' --output-on-failure
ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh /home/diogo/self-Zith/tests/run.sh
git -C /home/diogo/self-Zith diff --check -- src/ir/hir.zith src/ir/hir-verify.zith src/cache/cache.zith src/cache/zirl-container.zith src/cache/zirl-type-section.zith src/cache/zirl-canonical-types.zith src/cache/zirl-metadata.zith src/cache/zirl-decl-section.zith src/cache/zirl-template-section.zith src/cache/zirl-hir-artifact.zith tests/hir-contract.zith tests/canonical-types.zith tests/zirl-hir-artifact.zith docs/adr/0015-shared-hir-and-cross-dialect-cache.md docs/plans/step-6-2-contract-gaps.md docs/plans/shared-hir-interoperability.md docs/plans/shared-hir-interoperability-status.md docs/project-files.md
```

Do not mark Step 6.2 complete unless all producer fixtures, verifier fixtures,
projection fixtures, hydration round-trips, full harness checks, and the
path-scoped diff check pass. Do not change Step 6.6: the self-Zith frontend
adapter remains deferred.
