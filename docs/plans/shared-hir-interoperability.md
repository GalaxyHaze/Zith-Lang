# Shared HIR Interoperability Plan

**Objective:** Define and implement a frontend-neutral HIR contract in
`/home/diogo/Zith` that distinct Zith and Zith-- frontends can produce and
consume where semantics match, persist it through the cache, and reuse derived
VIR only when its schema and VM semantics match. Then port the stable contract
into `/home/diogo/self-Zith`.

**Preconditions:** This is a design and execution plan, not permission to edit
`/home/diogo/Zith`. That checkout is outside the writable roots; obtain
explicit approval before any build or write there. Re-run
`git -C /home/diogo/Zith status --short --branch` immediately before work.
The latest recorded status on 2026-10-23 reports a clean `main` at
`8f313dbf`, 22 commits ahead of `origin/main`; this is not a live status
claim, so revalidate before any sibling-repository operation. Preserve
existing worktree changes in both repositories. `/home/diogo/Zith/build`
already has a CMake build; its CTest config includes the focused targets named
below, and `test-vm-v2` uses skip return code 77. `/home/diogo/self-Zith` has
no CMake build, but now has the repository-owned test harness
`tests/run.sh`, documented in `tests/README.md`. It invokes
`/home/diogo/Zith/build/zithc`; `zithc test` type-checks fixtures, while the
harness separately runs VM and native behavioral fixtures.

**Scope boundary:** HIR is the canonical shared contract. Cache work in this
plan exists to persist and validate that contract; it is not a redesign of
frontend source-cache identity. VIR is derived and reusable only under matching
VIR-schema and VM-semantic versions. Codegen is a consumer: change its inputs
only where required to consume the stable HIR contract, and do not undertake a
separate codegen-module redesign in this workstream.

**Execution authorization:** This document is a plan, not approval to edit,
stage, commit, build, or test in `/home/diogo/Zith`. Each such operation needs
separate explicit approval. The 2026-10-23 status document reports that
Steps 1-5 were implemented and the sibling worktree was clean at `8f313dbf`;
this remains a recorded result, not live authorization. Before any sibling
operation, revalidate its status and diff. If the worktree is dirty, stop and
obtain an explicit disposition; do not stage, commit, stash, reset, clean, or
overwrite it.

**Files already changed by this design task:** `/home/diogo/self-Zith/CONTEXT.md`,
`/home/diogo/self-Zith/docs/architecture.md`,
`/home/diogo/self-Zith/docs/adr/0015-shared-hir-and-cross-dialect-cache.md`,
and this plan. The sibling repository also has the separate preparatory
codegen commit recorded above; it is not the shared-HIR implementation.
`/home/diogo/self-Zith` has unrelated modified and untracked user changes;
preserve them and never include them in implementation commits.

**Historical execution state (2026-10-22; superseded):** A prior session
reported an uncommitted Step 2 implementation in `/home/diogo/Zith`.
Its worktree then contained modifications to
`CMakeLists.txt`,
`src/cache/artifact-builder.cpp`,
`src/cache/cache-types.hpp`,
`src/codegen/codegen-emit-expr.cpp`,
`src/codegen/codegen-emit-stmt.cpp`,
`src/codegen/codegen.cpp`,
`src/hir/hir-attrs.hpp`,
`src/hir/hir-expr.hpp`,
`src/hir/hir-module.cpp`,
`src/hir/hir-module.hpp`,
`src/hir/hir-verify.cpp`,
`src/hir/hir-verify.hpp`,
`src/interp/hir-interpreter.cpp`,
`src/sema/hir-lower-block.cpp`,
`src/sema/hir-lower-call.cpp`,
`src/sema/hir-lower-expr-access.cpp`,
`src/sema/hir-lower-expr-value.cpp`,
`src/sema/hir-lower-modern.cpp`,
`src/sema/hir-lower-modern.hpp`,
`src/sema/hir-lower-stmt.cpp`,
`src/session/compilation-session.cpp`,
`src/session/native-link.cpp`,
`src/session/persistent-cache.cpp`,
`src/vm/hir-to-vm.cpp`,
`src/wasm/abi-hir.cpp`,
`src/zirl/zirl-code-section.cpp`,
`tests/test-cache.cpp`,
`tests/test-hir-lower-modern.cpp`,
and the untracked `tests/test-hir-compatibility.cpp`. These paths describe that
prior report only; they are not a current status claim or permission to absorb
any path. The later implementation commits and 2026-10-23 clean-worktree
report in `docs/plans/shared-hir-interoperability-status.md` supersede that
uncommitted state. Before any future sibling-repository operation, compare
live status and diff with the latest report. If the live worktree differs or
contains any unrelated change, stop and report it without staging or
modifying it.

The planned completion of Step 2 also needs the following paths because the
safety contract is persisted independently of ownership attributes, the cache
schema has changed, and the WASM encoder now accepts the completed HIR module
without a compilation session: `src/sema/hir-lower-expr-agg.cpp`,
`src/sema/hir-lower-types.cpp`, `src/wasm/abi-hir.hpp`,
`src/zirl/zirl-attrs-section.cpp`, `src/zirl/zirl-header.hpp`, and
`tests/test-wasm-contract.cpp`. Recheck them before editing; if any is already
modified outside the explicitly reviewed Step 2 diff, stop and report it
without touching it.

That prior session reported the HIR ID refactor and verifier implemented in
its pending diff, and the six focused tests `test-hir-compatibility`, `test-cache`,
`test-zirl-sections`, `test-hir-lower-modern`, `test-vm-v2`, and
`test-codegen` most recently passed. `test-wasm-contract` has not yet been
run, according to that session. These results are historical and must be
revalidated after the diff receives an explicit disposition. Do not commit it
until the safety contract below is represented explicitly and all Step 2
acceptance checks pass.

## Commit And Worktree Rules

- Keep one implementation commit per completed execution step in the
  repository being changed. Do not amend existing commits, push, or rebase.
- Before any operation in `/home/diogo/Zith`, run
  `git -C /home/diogo/Zith status --short --branch` and inspect the relevant
  diff. Require a clean worktree; stop if any change appears and ask for an
  explicit disposition. The earlier uncommitted Step 2 report is historical
  and does not authorize resuming or absorbing a diff.
- Never stage paths outside the completed step's explicit allowlist.
- After a step passes its tests, stage only that step's changed files, run
  `git diff --cached --check` from the repository being changed, and commit
  with the message specified by that step. If tests or the staged diff check
  fail, do not commit.
- After each commit, verify it with `git show --stat --oneline HEAD` and
  `git status --short --branch` from that repository.
- Request separate explicit approval for writes and for build/test commands
  under `/home/diogo/Zith`; approval for one does not imply the other.
- In `/home/diogo/self-Zith`, do not stage, commit, or modify a path unless
  the current port step explicitly assigns it. Never use
  `git reset --hard`, `git checkout --`, or `git clean`.

## Decisions The Implementation Must Preserve

- Keep source/frontend cache identity dialect-specific. Never treat equal
  source bytes or matching syntax as proof of equal language semantics.
- Share a persisted HIR artifact only when the declared semantic compatibility
  profile and canonical module identity match. The frontend may still need to
  parse and lower source before such an HIR cache lookup.
- Make a completed HIR module plus language-neutral IR tables sufficient for
  codegen, VIR lowering, and cache serialization; do not require callers to
  pass AST, `SemaPipeline`, `SymbolTable`, or session state.
- Use the verified `HirModule` as the shared seam. Keep one lowering adapter
  per frontend; do not add a virtual frontend superclass or a pass-through
  interface solely to make the adapters look uniform.
- Preserve required runtime checks as HIR semantics. Store Zith proof results
  separately as optional facts; absent facts mean "keep the check or recompute
  the proof," never "assume safe." Preserve `raw` as explicitly unchecked.
- Persist canonical module, type, symbol, and string identities; never persist
  process-local AST, session, interner, or symbol-table indices as identities.
- Treat HIR as canonical. Treat VIR as a derived cache section that is reusable
  only for a matching VIR schema and VM semantic version. Do not cross-share
  LLVM/object/assembly/backend outputs.
- Version the container, HIR schema, and VIR schema independently. Skip an
  unknown optional section only when its optional status is explicit; reject
  an unknown required semantic feature.

## Execution Steps

### 1. [DONE] Capture and verify the sibling baseline

**Goal:** Verify the sibling checkout state and establish a passing focused
test baseline before changing shared interfaces.

**Sub-steps:**
1. Run `git -C /home/diogo/Zith status --short --branch`.
2. If any tracked or untracked change appears, stop; do not stash, reset,
   clean, commit, or overwrite it. Report the paths and ask how to proceed.
3. If the checkout is clean, confirm its branch and commit relationship with
   the status output; do not push or rebase.
4. Request explicit approval before building or running commands that write
   under `/home/diogo/Zith/build`.
5. After approval, build the focused targets:

   ```sh
   cmake --build /home/diogo/Zith/build --target \
     test-cache test-zirl-sections test-cache-entry \
     test-hir-lower-modern test-vm-v2 test-codegen
   ```

6. Run:

   ```sh
   ctest --test-dir /home/diogo/Zith/build \
     -R '^(test-cache|test-zirl-sections|test-cache-entry|test-hir-lower-modern|test-vm-v2|test-codegen)$' \
     --output-on-failure
   ```

**Recorded result (2026-10-22):** The sibling worktree was clean on `main`,
and the six-test focused suite passed after the adapter fix. A later status
check confirmed HEAD `1d4e83c9`, 15 commits ahead of `origin/main`.

**Expected output on revalidation:** The build exits 0; CTest lists six
selected tests and reports each as passed or reports `test-vm-v2` skipped
through its configured return code 77.

**Failure checks:** If the checkout is dirty, stop without touching it. If
the owner has not approved build/test execution, stop before invoking either
command. If building fails, save the complete compiler output and stop before
editing. If a baseline test fails, record the test name and full failure
output; do not attribute it to this design or modify code to hide it.

**Success criteria:** The recorded baseline is confirmed unchanged before
implementation, and every selected test is either passed or explicitly
skipped by its existing CTest rule.

### 2. [PAUSED: AUDIT REQUIRED] Remove frontend-owned identities and verify the HIR contract

**Goal:** A completed `hir::HirModule` contains all semantic information
required by its consumers without consulting the source AST or frontend
session.

**Sub-steps:**
1. Revalidate the current worktree before resuming:

   ```sh
   git -C /home/diogo/Zith status --short --branch
   git -C /home/diogo/Zith diff --name-only
   ```

   Compare the live path set with the "Previously recorded execution state".
   If it differs, stop and report every unexpected path; do not stage, stash,
   reset, clean, or overwrite it. Even if it matches, stop after the audit and
   obtain the user's explicit disposition before resuming the diff.
2. Obtain explicit user approval before any further write under
   `/home/diogo/Zith`. Obtain separate explicit approval before any build or
   test command that writes under `/home/diogo/Zith/build`. Earlier approval
   does not authorize new operations.
3. Re-read the pending diff and confirm every changed hunk belongs to Step 2:

   ```sh
   git -C /home/diogo/Zith diff -- \
     src/hir src/sema src/session src/cache src/codegen src/vm src/wasm \
     src/zirl tests CMakeLists.txt
   ```

   If any hunk is unrelated, stop and report its path and purpose; do not
   discard or stage it.
4. Inventory HIR includes and fields with:

   ```sh
   rg -n '#include|ast::|symbols::|session::|sema::|FileId|Span|InternedId|TypeId' \
     /home/diogo/Zith/src/hir
   ```

   Record each match as semantic HIR data, debug/source metadata,
   process-local identity, or frontend implementation dependency. The
   confirmed direct dependencies include `ast::DeclId decl_id` and
   `symbols::SymId sym_id` in `hir::HirFunction`,
   `symbols::SymId resolved_fn` in `hir::HirCall`, and symbol IDs in
   `hir::HirVTable::slots`. `foreignAdapterName`, function names, and
   parameter names are interner IDs; `HirTypeId` aliases `types::TypeId`.
   Confirm these against the checkout before changing them.
5. Inventory actual consumers before deciding which fields move:

   ```sh
   rg -n 'decl_id|sym_id|fnSpan|param_names|foreignAdapterName|machineId|machineReturnType' \
     /home/diogo/Zith/src
   ```

   For every field, record the consumer and whether it is required for
   semantics, diagnostics/debugging, codegen, or cache ABI extraction. Apply
   these replacement constraints:
   - Keep an explicit function-linkage/definition classification in HIR.
     `vm/hir-to-vm.cpp` currently uses `decl_id` to distinguish some
     empty-body declarations, while AST declaration IDs are not restored by
     cache hydration.
   - Replace persisted generic-instantiation `decl_id` with a canonical
     template/module identity; `cache/artifact-builder.cpp` currently copies
     the numeric declaration ID into `InstantiationRecord`.
   - Replace `sym_id`, `HirCall::resolved_fn`, and vtable slot references
     together. VM call/vtable lowering matches them against `HirFunction`,
     `native-link.cpp` uses them to find adapter calls, and cache/WASM
     serializers currently write numeric values. First classify local,
     external, and cross-module references; use HIR-owned references in
     memory and canonical module-qualified references at the persistence
     seam. Never renumber these independently.
   - Treat `fnSpan` as source/debug metadata, not semantic identity. Keep it
     only in an explicitly optional source-map/debug payload or omit it from
     canonical HIR serialization.
   - Convert interner/type-table indices to canonical string/type-table
     references during serialization; never use raw process-local numbers as
     cross-compiler identities.
6. Inspect `/home/diogo/Zith/src/hir/hir-expr.hpp`,
   `/home/diogo/Zith/src/hir/hir-module.hpp`,
   `/home/diogo/Zith/src/hir/hir-types.hpp`, and
   `/home/diogo/Zith/src/hir/hir-attrs.hpp`. Specify separately the required
   runtime-check obligations, optional proof/refinement facts, and explicitly
   unchecked `raw` operations. Use this fixed HIR contract:
   - `HirSafetyMode::RequiredCheck` means a backend or a later safety-lowering
     pass must preserve the operation's language-defined failure behavior.
   - `HirSafetyMode::Guarded` means explicit control flow in the same verified
     HIR function has already performed the check before this operation.
   - `HirSafetyMode::Intrinsic` means the operation is safe by construction,
     such as a validated fixed struct-field projection or a whole-array view.
   - `HirSafetyMode::Raw` means the source explicitly requested unchecked
     behavior. It is not a missing check, failed proof, or default.
   - Missing safety metadata resolves conservatively to `RequiredCheck`; it
     must never resolve to `Intrinsic`, `Guarded`, or `Raw`.
   - Store proof facts separately in an expression-indexed `HirSafetyAttrs`
     table. Each fact identifies its proof kind and the exact operand IDs it
     covers. A proof may discharge a required check only when its kind and all
     operand IDs match that operation; absent, malformed, or mismatched facts
     leave the check required. This step adds the representation and validates
     it; it does not add an optimizer that trusts unverified proof claims.
   Apply these fixed classification rules:
   - A check required by language semantics remains an obligation whether or
     not a proof fact exists.
   - A proof fact is optional metadata. It may justify elision only for the
     exact operation and operands it proves; absent, invalid, or unknown facts
     retain the check or trigger proof recomputation.
   - `raw` is an explicit unchecked operation, not a missing check flag and
     not a proof result.
   - An operation requiring no check by construction is distinct from both a
     required check and explicit `raw`.

   Trace each construction and consumer of these cases before editing:
   `HirIndex` at `sema/hir-lower-expr-access.cpp::lowerIndex`;
   `HirMakeSlice` at `lowerSliceRange` and array-to-slice lowering in
   `sema/hir-lower-expr-value.cpp`;
   `HirOpaqueCast` at `lowerCast` and `lowerOptionalPayload`;
   `HirUnionCast` at `lowerCast`, union-literal construction, and narrowed
   local reads; and `HirField` used for optional payload extraction. Record
   whether each site is explicitly raw, guarded by already-emitted control
   flow, statically safe by construction, or still requires a runtime check.
   Do not use one overloaded `checked` boolean for these distinct meanings.
7. Inspect `/home/diogo/Zith/src/hir/hir-verify.cpp` and its callers. Define
   verifier invariants and failure diagnostics before implementing
   `hir::HirVerifier::verify()`. Its current implementation returns `true`
   without validation and has no callers. Cover expression and terminator
   references, block/control-flow targets, slots, types, function references,
   vtable slots, and required safety obligations. Reject contradictory or
   malformed safety metadata. Never infer safety from a default field value.
8. Add explicit `HirSafetyMode` semantics to `HirIndex`, `HirField`,
   `HirMakeSlice`, `HirUnionCast`, and `HirOpaqueCast`; do not retain an
   overloaded `checked` boolean as the semantic contract. Keep proof facts in
   `HirSafetyAttrs`, separate from each operation's mode. Populate the
   classification at every Zith-- lowering site, including field projections
   in `hir-lower-expr-agg.cpp`, union casts in `hir-lower-types.cpp`, guarded
   optional payload extraction, and raw expressions. Update codegen,
   interpreter, VM lowering, WASM ABI encoding/decoding, and ZIRL artifact
   building/hydration so each mode and proof fact survives and no consumer
   treats missing proof as safe.
9. Refactor HIR declarations and implementation only after the inventory and
   invariants are recorded. Keep compact HIR-owned references if useful, but
   translate them to stable canonical identities at the serialization seam.
   Replace AST-dependent behavior with explicit HIR semantics, not sentinel
   interpretations of process-local IDs.
10. Add a common verifier invocation after `lowerStage()` and before
   `codegenStage()` in `CompilationSession::runTo()` in
   `/home/diogo/Zith/src/session/compilation-session.cpp`. Do not call it only
   inside `lowerStage()`: that function returns immediately on a hydrated
   cache hit, so verification must cover both cold-lowered and cached HIR.
   Ensure verifier diagnostics stop both codegen and cache materialization.
11. Extend `/home/diogo/Zith/tests/test-hir-compatibility.cpp` and register it
   in `/home/diogo/Zith/CMakeLists.txt`. Extend
   `/home/diogo/Zith/tests/test-wasm-contract.cpp` for the WASM HIR envelope.
   Reuse setup patterns from
   `/home/diogo/Zith/tests/test-hir-lower-modern.cpp`; cover malformed HIR
   references from each reference category, every safety classification,
   required-check behavior with absent proof metadata, explicit `raw`, valid
   and operand-mismatched proof facts, and ZIRL/WASM serialization round trips.
   Assert that absent/mismatched proof facts leave `RequiredCheck` unchanged.
   Test unknown required cache features in Step 4, where the format reader can
   reject them.
12. Resolve each pending semantic audit before staging:
   - In `/home/diogo/Zith/src/sema/hir-lower-expr-value.cpp`, inspect
     `lowerIsNull` and classify its optional-payload `HirField` from the
     emitted control flow. Set its `HirSafetyMode` explicitly; do not rely on
     the default mode. The optional aggregate's discriminant read is
     `Intrinsic`; add an assertion to
     `/home/diogo/Zith/tests/test-hir-lower-modern.cpp`.
   - In `/home/diogo/Zith/src/sema/hir-lower-call.cpp`, trace optional-payload
     extraction to the corresponding emitted guard. Make the operation's
     safety mode agree with that control flow and add a regression assertion.
   - In `/home/diogo/Zith/src/hir/hir-verify.cpp`, reject `Guarded` operations
     unless the same function's verified control flow proves the matching
     guard dominates the operation. Build the CFG from each block's branch and
     jump terminator, compute block dominators, and require a matching check
     on the selected predecessor edge; block dominance without a matching
     check is insufficient. Reject proof attributes attached to expressions
     that cannot consume that proof kind. Add positive and negative verifier
     tests in `/home/diogo/Zith/tests/test-hir-compatibility.cpp`, including a
     guarded payload read on the true edge, a read on the false edge, a
     non-dominating guard, and proofs attached to unsupported expression
     kinds.
   - Trace every `HirIndex` and `HirMakeSlice` mode through
     `/home/diogo/Zith/src/codegen/codegen-emit-expr.cpp`,
     `/home/diogo/Zith/src/codegen/codegen-emit-agg.cpp`,
     `/home/diogo/Zith/src/interp/hir-interpreter.cpp`,
     `/home/diogo/Zith/src/vm/hir-to-vm.cpp`,
     `/home/diogo/Zith/src/vm/typed-ir.hpp`,
     `/home/diogo/Zith/src/vm/typed-ir-dump.cpp`, and
     `/home/diogo/Zith/src/vm/vm-v2.cpp`. `RequiredCheck` on a bounded
     array/slice index must preserve the source-level bounds failure;
     `Guarded` relies on a verifier-approved dominating check;
     `Intrinsic` is reserved for a construction whose bounds are known; and
     `Raw` skips logical bounds checks while the VM still rejects reads beyond
     its physical linear memory. Do not treat a VM memory-range check as proof
     of a logical array/slice bound. Keep pointer indexing's existing
     pointer/null semantics separate from array/slice bounds. Add focused
     cases to `/home/diogo/Zith/tests/test-vm-v2.cpp` that distinguish a
     logical bound from the VM allocation boundary, and add native codegen
     coverage for `RequiredCheck` and `Raw`.
   - In `/home/diogo/Zith/src/cache/artifact-builder.cpp`, reject the invalid
     `InstantiationRecord::decl_id` sentinel as a persisted identity. Store
     the canonical template/module identity required by this step, or stop
     and move that cache-only change to Step 4 by removing it from the Step 2
     diff and allowlist through an explicitly reviewed disposition.
   - In `/home/diogo/Zith/src/session/persistent-cache.cpp` and
     `/home/diogo/Zith/src/vm/hir-to-vm.cpp`, verify that declaration versus
     definition is represented explicitly and is not inferred solely from
     `blocks.empty()`. Add a round-trip and lowering test for each
     classification.
13. After separate approval for build/test execution, build and run the HIR,
   WASM, cache-entry, VM bounds, and baseline contract tests:

   ```sh
   cmake --build /home/diogo/Zith/build --target \
     test-cache test-zirl-sections test-cache-entry test-hir-compatibility \
     test-hir-lower-modern test-wasm-contract test-vm-v2 test-codegen
   ctest --test-dir /home/diogo/Zith/build \
     -R '^(test-cache|test-zirl-sections|test-cache-entry|test-hir-compatibility|test-hir-lower-modern|test-wasm-contract|test-vm-v2|test-codegen)$' \
     --output-on-failure
   ```

   If any audit item changes code after these commands, rerun this complete
   build and CTest command before staging.

**Expected output:** The dependency inventory is resolved, the verifier
accepts valid modules and rejects malformed ones, and the new tests demonstrate
that absent or mismatched proof metadata does not silently remove a required
check. Guarded operations are accepted only when a matching check dominates
them along the selected CFG edge. Explicit `raw`, required-check, guarded,
intrinsically safe, and proof-backed cases remain distinguishable after both
ZIRL and WASM serialization/hydration. Native and VM paths preserve the
array/slice logical-bounds contract, and VM physical-memory protection remains
active for raw operations. The ZIRL format version is incremented and the
cache test asserts the new exact version.

**Failure checks:** If an HIR consumer still needs source AST or session data,
stop and move the missing semantic datum into HIR or an explicitly supplied
debug/target input before proceeding. If a field has multiple meanings across
consumers, do not collapse it; report the consumers and revise the contract.
If a type or symbol ID cannot be classified as frontend-neutral, stop and
record that dependency rather than serializing its numeric value. If the
verifier cannot recognize a `Guarded` operation's exact runtime predicate,
reject the module and report the predicate shape; do not accept block
dominance alone. If a VM raw-index test cannot distinguish the logical
container bound from the physical memory boundary, revise the test fixture
before changing the VM semantics.

**Success criteria:** The new test passes; verifier execution covers both
cold and hydrated HIR; and the HIR dependency audit shows no frontend/session
implementation types in the shared HIR interface. AST or source identities
may remain only in explicitly non-semantic debug metadata with defined
persistence behavior. All eight Step 2 CTest cases pass, except that
`test-vm-v2` may be skipped only through its existing return-code-77 rule.
Every pending semantic audit above is resolved and covered by a test. A
negative `Guarded`-dominance fixture is rejected, a positive fixture is
accepted, and VM/native tests prove that `Raw` does not silently regain a
logical bounds check.

**Commit:** After all checks pass, confirm `git diff --name-only` still matches
the Step 2 allowlist, then commit only these paths:
`CMakeLists.txt`,
`src/cache/artifact-builder.cpp`,
`src/cache/cache-types.hpp`,
`src/codegen/codegen-emit-agg.cpp`,
`src/codegen/codegen-emit-expr.cpp`,
`src/codegen/codegen-emit-stmt.cpp`,
`src/codegen/codegen.cpp`,
`src/hir/hir-attrs.hpp`,
`src/hir/hir-expr.hpp`,
`src/hir/hir-module.cpp`,
`src/hir/hir-module.hpp`,
`src/hir/hir-verify.cpp`,
`src/hir/hir-verify.hpp`,
`src/interp/hir-interpreter.cpp`,
`src/sema/hir-lower-block.cpp`,
`src/sema/hir-lower-call.cpp`,
`src/sema/hir-lower-expr-agg.cpp`,
`src/sema/hir-lower-expr-access.cpp`,
`src/sema/hir-lower-expr-value.cpp`,
`src/sema/hir-lower-modern.cpp`,
`src/sema/hir-lower-modern.hpp`,
`src/sema/hir-lower-stmt.cpp`,
`src/sema/hir-lower-types.cpp`,
`src/session/compilation-session.cpp`,
`src/session/native-link.cpp`,
`src/session/persistent-cache.cpp`,
`src/vm/hir-to-vm.cpp`,
`src/vm/typed-ir.hpp`,
`src/vm/typed-ir-dump.cpp`,
`src/vm/vm-v2.cpp`,
`src/wasm/abi-hir.cpp`,
`src/wasm/abi-hir.hpp`,
`src/zirl/zirl-attrs-section.cpp`,
`src/zirl/zirl-code-section.cpp`,
`src/zirl/zirl-header.hpp`,
`src/zirl/zirl-instantiation-section.cpp`,
`tests/test-cache.cpp`,
`tests/test-hir-compatibility.cpp`,
`tests/test-hir-lower-modern.cpp`,
`tests/test-codegen.cpp`,
`tests/test-vm-v2.cpp`,
`tests/test-wasm-contract.cpp`, and
`tests/test-zirl-sections.cpp`
as `refactor(hir): define verified frontend-neutral contract`. If the safety
metadata requires another path, stop and revise this allowlist before editing
or staging that path.

### 3. Adapt the Zith-- lowering to the shared HIR contract

**Goal:** The existing Zith-- lowering produces verified HIR without making
the shared HIR interface depend on Zith-- frontend implementation types.

**Sub-steps:**
1. Treat `zith::sema::modern::HirLowerModern::run()` in
   `/home/diogo/Zith/src/sema/hir-lower-modern.cpp` as the Zith-- adapter;
   change it to populate only the shared HIR contract from Step 2.
2. Keep AST traversal, source diagnostics, and semantic decisions inside that
   adapter. Do not move frontend AST or sema types into
   `/home/diogo/Zith/src/hir/`.
3. Do not add a shared frontend superclass or a pass-through HIR builder solely
   to make the two adapters look alike. Each adapter may construct the same
   HIR data contract using its own lowering implementation.
4. Add adapter-level tests to
   `/home/diogo/Zith/tests/test-hir-lower-modern.cpp` for common construct
   lowering and preservation of the Step 2 safety metadata.
5. Defer two-frontend byte equality and cache cross-read claims until Step 6
   has a real Zith HIR producer and a runnable test path in `self-Zith`.
6. Build and run the adapter test:

   ```sh
   cmake --build /home/diogo/Zith/build --target test-hir-lower-modern
   ctest --test-dir /home/diogo/Zith/build \
     -R '^test-hir-lower-modern$' --output-on-failure
   ```

**Expected output:** The Zith-- adapter produces verified HIR and its
construct/safety conformance tests pass. Two-frontend byte equality is not
claimed at this stage.

**Failure checks:** If equivalent constructs produce different HIR, report the
first differing node/field and stop. Do not normalize away a difference until
its language-semantic cause is understood. Do not claim cross-frontend
compatibility in this step because the Zith HIR producer does not yet exist in
`self-Zith`.

**Success criteria:** The Zith-- adapter passes its conformance fixtures and
the verified HIR seam is frontend-independent. Cross-frontend acceptance is
reserved for Step 6.

**Commit:** After all checks pass, commit only the Zith-- adapter and its
conformance-test files changed by this step as
`refactor(sema): lower Zith-- into shared HIR`.

### 4. Decouple cache identity and persist canonical HIR

**Goal:** The cache can load canonical HIR produced by either frontend without
depending on `session::CacheKey` or process-local IDs.

**Sub-steps:**
1. Inspect
   `/home/diogo/Zith/src/cache/cache.hpp`,
   `/home/diogo/Zith/src/cache/cache-entry.hpp`,
   `/home/diogo/Zith/src/cache/cache-types.hpp`,
   `/home/diogo/Zith/src/cache/artifact-builder.hpp`, and
   `/home/diogo/Zith/src/session/frontend-context.hpp`. Record which values
   identify a source/frontend result, which identify a canonical HIR module,
   and which are process-local. Do not conflate the source cache key with the
   cross-dialect HIR compatibility identity.
2. Define and review the identity/version compatibility matrix before coding:
   source/frontend identity mismatch must not imply HIR incompatibility;
   semantic-profile or canonical-module mismatch must reject HIR reuse;
   HIR-schema mismatch must reject HIR hydration; VIR/VM mismatch must only
   invalidate derived VIR. Keep frontend identity in frontend-specific cache
   records, not as a required part of the shared HIR semantic payload.
3. Add cache-owned identity types in
   `/home/diogo/Zith/src/cache/cache-identity.hpp` for dialect/frontend
   identity, semantic compatibility profile, canonical module identity, and
   content fingerprints.
4. Refactor `cache::Store` in
   `/home/diogo/Zith/src/cache/cache.hpp` and
   `/home/diogo/Zith/src/cache/cache.cpp`, plus
   `/home/diogo/Zith/src/cache/cache-entry.hpp` and
   `/home/diogo/Zith/src/cache/cache-entry.cpp`, so persisted cache identity
   accepts cache-owned values instead of depending on
   `session/frontend-context.hpp`.
5. Refactor `cache::ArtifactBuilder` in
   `/home/diogo/Zith/src/cache/artifact-builder.hpp` and
   `/home/diogo/Zith/src/cache/artifact-builder.cpp` to consume the completed
   HIR module and its HIR-owned metadata, not `SymbolTable`, session cache
   identity, or frontend lowering state.
6. Update `/home/diogo/Zith/src/cache/cache-types.hpp` so serialized HIR
   references use canonical IDs and safety obligations/proof facts have
   separate fields.
7. Inspect the current section table and parsing rules in
   `/home/diogo/Zith/src/zirl/zirl-header.hpp`,
   `/home/diogo/Zith/src/zirl/zirl-header.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-reader.cpp`, and
   `/home/diogo/Zith/src/zirl/zirl-writer.cpp` before choosing how required
   features and optional sections are encoded. Then update those files and the
   affected section readers/writers, including:
   `/home/diogo/Zith/src/zirl/zirl-code-section.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-attrs-section.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-type-section.cpp`,
   `/home/diogo/Zith/src/zirl/zirl-decl-section.cpp`, and
   `/home/diogo/Zith/src/zirl/zirl-instantiation-section.cpp`, to carry
   independent container/HIR schema versions and explicit required/optional
   feature data.
8. Make old or semantically incomplete artifacts a clean cache miss. Make
   unknown optional metadata skippable only when the format can prove it is
   optional; reject unknown required features.
9. Extend `/home/diogo/Zith/tests/test-cache-entry.cpp` and
   `/home/diogo/Zith/tests/test-zirl-sections.cpp` for canonical ID
   round-trip, legacy-version invalidation, unknown optional data behavior,
   unknown required feature rejection, and the reader's no-partial-hydration
   behavior.
10. Build and run the cache/ZIRL tests:

    ```sh
    cmake --build /home/diogo/Zith/build --target \
      test-cache test-cache-entry test-zirl-sections
    ctest --test-dir /home/diogo/Zith/build \
      -R '^(test-cache|test-cache-entry|test-zirl-sections)$' \
      --output-on-failure
    ```

**Expected output:** An explicit compatibility matrix governs cache reuse;
old-format artifacts miss cleanly; malformed or unknown-required artifacts
are rejected without hydrating partial HIR. Cross-frontend reading is not
claimed until the self-Zith adapter exists and Step 6 exercises both readers.

**Failure checks:** If a process-local ID appears in serialized identity or
HIR references, fail the canonical-ID test and fix the serializer mapping. If
the format cannot safely skip optional data, classify that data as required
and reject the artifact instead of guessing. If the source cache key is still
used as the sole HIR compatibility key, stop and revise the identity matrix.

**Success criteria:** Cache/ZIRL tests pass, and a source/header audit shows
that the shared persisted HIR identity no longer depends on
`session/frontend-context.hpp`. Frontend-specific source-cache identity
remains dialect-specific.

**Commit:** After all checks pass, commit only the cache identity, artifact,
ZIRL reader/writer, and cache/ZIRL test files changed by this step as
`refactor(cache): persist canonical shared HIR`.

### 5. Version and validate derived VIR; adapt consumers only as required

**Goal:** VIR reuse and backend lowering depend only on stable HIR contracts
and explicit target/VM configuration.

**Sub-steps:**
1. Confirm whether VIR is currently serialized:

   ```sh
   rg -n -i 'vir|typed.?ir|vm semantic|schema.?version' \
     /home/diogo/Zith/src/cache /home/diogo/Zith/src/zirl \
     /home/diogo/Zith/src/vm
   ```

   The 2026-10-22 inventory found VM typed-IR definitions and dumps but no VIR
   persistence in cache/ZIRL. If that remains true, treat VIR serialization as
   a new optional cache feature, not an existing section refactor.
2. In `/home/diogo/Zith/src/vm/typed-ir.hpp`, define the VIR schema identity
   separately from the HIR schema and define the VM semantic version separately
   from both.
3. In `/home/diogo/Zith/src/vm/hir-to-vm.hpp` and
   `/home/diogo/Zith/src/vm/hir-to-vm.cpp`, make
   `lowerModule()` consume only HIR plus shared type/string data and explicit
   VM configuration; remove any remaining frontend/session lookup.
4. Add the VIR section to ZIRL as derived optional data only after the cache
   format work in Step 4 provides an explicit optional-section mechanism.
   Reuse VIR only when both its schema and the VM semantic version match;
   otherwise discard it and lower again from valid HIR.
5. Audit codegen consumers for remaining dependencies on frontend-owned
   semantic data. Change only the specific input or HIR field required to
   consume the verified shared contract; do not redesign the codegen module,
   `CodeGen::emit()`, or `CodeGenType` as a separate workstream. If a required
   semantic datum is absent from HIR, add it to the HIR contract in Step 2,
   update that step's file allowlist, and rerun its acceptance checks.
6. Keep LLVM IR, object files, and assembly outside the shared cache contract.
7. Extend `/home/diogo/Zith/tests/test-vm-v2.cpp` and
   `/home/diogo/Zith/tests/test-cache-entry.cpp` with matching-version VIR
   reuse and mismatched-version VIR regeneration tests. Add a focused codegen
   regression only if the consumer adaptation in sub-step 5 changes observable
   behavior.
8. Build and run the VM/cache tests:

   ```sh
   cmake --build /home/diogo/Zith/build --target \
     test-vm-v2 test-cache-entry
   ctest --test-dir /home/diogo/Zith/build \
     -R '^(test-vm-v2|test-cache-entry)$' --output-on-failure
   ```

**Expected output:** A matching VIR artifact loads only when VIR schema and VM
semantic versions match; mismatched VIR is discarded and regenerated from
valid HIR. Existing codegen behavior remains unchanged except for any narrowly
required HIR-consumer adaptation. Backend outputs remain outside the
cross-dialect cache.

**Failure checks:** If VM behavior changes without a VM semantic-version
bump, stop and add the bump before accepting cache reuse. If codegen needs
frontend-owned metadata, add that semantic metadata to HIR and test it rather
than adding a frontend dependency back. If the cache format cannot skip a
derived section safely, stop and revise the format before persisting VIR.

**Success criteria:** VM/cache tests pass; any codegen regression test required
by a changed consumer passes; no object/backend artifact is accepted as a
cross-dialect cache hit.

**Commit:** After all checks pass, commit only the VIR schema/lowering,
strictly necessary HIR-consumer adaptation, and corresponding test files
changed by this step as `refactor(ir): version derived VIR`.

### 6. Port and validate the shared contract in self-Zith

**Goal:** self-Zith can encode, read, verify, and consume a complete shared
HIR artifact using caller-owned storage, without depending on Zith-- frontend
types. This step does not claim frontend interoperability until both real
frontend adapters exist and pass the same fixtures.

**Current baseline:** On 2026-10-23, checkpoint
`9d56fe2 wip(cache): compose typed HIR artifact sections` was committed in
`/home/diogo/self-Zith`. The typed writer calls five section codecs and the
generic envelope writer, but
`ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh
tests/run.sh` exits 2 because the native `tests/zirl-hir-artifact.zith`
fixture receives `ZirlArtifactError.InvalidSections`. The harness is
documented in `/home/diogo/self-Zith/tests/README.md` and implemented by
`/home/diogo/self-Zith/tests/run.sh`; `zithc test` type-checks fixtures and
does not execute their functions. The worktree has unrelated modified and
untracked files; preserve them.

**Authorization boundary:** Work in `/home/diogo/self-Zith` only. The sibling
`/home/diogo/Zith` checkout was last reported clean at `8f313dbf` on `main`
on 2026-10-23, but revalidate it before any future work. This plan does not
authorize writes, builds, tests, staging, or commits in that checkout. If a
missing shared semantic field requires changing the sibling contract, stop
and obtain explicit authorization before changing either the sibling
implementation or its tests. Do not use `git clean`, `git reset`, broad
staging, or generated-output cleanup.

#### 6.1 Diagnose and repair the failing typed writer

**Goal:** The existing empty-HIR fixture identifies whether failure occurs in
the typed codecs, five-section assembly, or envelope, and the native writer
test exits 0.

**Sub-steps:**
1. Read `write_zirl_hir_artifact()` in
   `/home/diogo/self-Zith/src/cache/zirl-hir-artifact.zith`,
   `valid_artifact_sections()` and `write_zirl_artifact()` in
   `/home/diogo/self-Zith/src/cache/zirl-artifact.zith`, and
   `exercise_hir_artifact_writer()` in
   `/home/diogo/self-Zith/tests/zirl-hir-artifact.zith`.
2. Add fixture assertions for the writer's count preconditions and each
   section codec's `failed` state. Do not dereference
   `storage.sections[index]` in the fixture: direct raw indexing of that
   section array previously triggered a compiler ICE.
3. In the same fixture, construct a literal five-entry section array with
   required IDs `1, 2, 3, 4, 5`, each `flags == 1`, Metadata schema `2`, and
   other required schemas `1`. Pass it to `write_zirl_artifact()` and then
   `read_zirl_artifact()`. A successful read proves the envelope accepts the
   section order/schema without the problematic raw-index reads.
4. Correct only the boundary shown by the checks. Do not weaken
   `valid_artifact_sections()`, omit a required section, or turn the failure
   into a nominal success without satisfying the envelope contract.
5. After the typed writer succeeds, pass its output to `read_zirl_artifact()`
   and assert successful validation; use this public behavior to verify its
   generated required section table without indexing `storage.sections`.
6. Run:

   ```sh
   ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp \
     sh /home/diogo/self-Zith/tests/run.sh
   ```

7. If the section assertions pass but the harness still fails, preserve the
   exact failing test, command, and exit status. Do not infer behavioral
   success from `zithc test`.

**Expected output:** The fixture pinpoints the previous failure; the empty
artifact has five valid required sections; the full harness exits 0.

**Failure checks:** If a section writer fails, inspect its `write_*_section()`
validation and fixture input, then repair that input or codec defect. If the
literal envelope round-trip fails, compare the literal entries with
`valid_artifact_sections()` and `validate_zirl_sections()`. If the literal
envelope round-trip succeeds but the typed writer still returns
`InvalidSections`, check its count preconditions and codec results; add a
structured failure-stage result to the typed writer if needed to distinguish
its remaining branches. Do not read the section array through raw indexing
in the fixture. If the compiler ICEs or fails to lower another assertion,
stop and report the diagnostic; do not replace the fixture with a weaker
check.

**Success criteria:** The exact harness command exits 0 and the fixture
checks the literal five-entry envelope, reads the typed writer's output
successfully, and observes a nonzero artifact size without indexing the
writer's internal section array.

**Commit:** Commit only the writer fix, its assertions in
`tests/zirl-hir-artifact.zith`, and any corresponding
`docs/project-files.md` entry as
`fix(cache): validate typed HIR artifact composition`.

#### 6.2 Reconcile HIR fields with declaration and template records

**Goal:** Every serialized declaration/template field has a lossless source in
the shared HIR contract or an explicitly approved contract extension.

**Sub-steps:**
1. Compare `HirDeclaration` and `HirModule` in
   `/home/diogo/self-Zith/src/ir/hir.zith` with
   `ZirlDeclarationRecord` in `src/cache/zirl-decl-section.zith` and
   `ZirlTemplateRecord` / `ZirlGenericParamRecord` in
   `src/cache/zirl-template-section.zith`.
2. Create a field-by-field mapping table in this plan or a focused contract
   document. Name the exact HIR source and conversion for every field; mark
   any field with no source as unresolved. Explicitly audit `is_extern`,
   `template_index`, `body_function_index`, `field_name_ids`,
   `field_type_ids`, `method_declaration_indices`, generic-parameter bounds,
   `canonical_field_order`, and `trait_type_ids`.
3. Do not infer `is_extern`, template membership, body indices, field order,
   trait relationships, or empty/default records from absent data.
   `HirDeclaration.members` may populate a cache field only after its meaning
   and ordering are proven to match that field.
4. For every unresolved field, stop implementation and prepare a contract
   proposal naming its semantic meaning, HIR owner, HIR schema impact, ZIRL
   section schema impact, and a fixture proving the mapping. Obtain user
   authorization before changing `/home/diogo/Zith`.
5. After an approved contract change is implemented and committed upstream,
   port only the corresponding shared field into self-Zith and update
   `docs/adr/0015-shared-hir-and-cross-dialect-cache.md`,
   `docs/project-files.md`, and affected schema versions in that same slice.

**Expected output:** The mapping has no unresolved fields before projection
starts, or this sub-step stops with a concrete contract proposal and no
lossy serialization code.

**Failure checks:** If a proposed mapping depends on a Zith-- AST, resolver,
session, or process-local index not represented in HIR, reject it and stop for
a shared-contract change. If changing the contract requires writing to
`/home/diogo/Zith` without approval, do not edit it; report the needed
authorization and paths.

**Success criteria:** Every declaration/template record field has a
documented, tested HIR source; no projection assigns guessed values.

**Commit:** Commit a resolved mapping note only with implementation/tests
that exercise it. Do not assert the mapping is complete while any field is
unresolved.

#### 6.3 Build the complete HIR-to-section projection

**Goal:** A verified `HirModule`, together with explicit artifact identity and
path metadata, deterministically produces all five typed HIR sections using
caller-owned storage.

**Sub-steps:**
1. Treat `ZirlArtifactIdentity` as envelope input, not as a `HirModule` field.
   Keep canonical path, module name, semantic profile/version, source
   fingerprints, ABI values, optional features, counts, and dependencies in
   caller-supplied identity data. Treat paths as explicit Metadata input; do
   not synthesize identity or paths from HIR.
2. Call `verify_hir()` before projection. On verification failure, return a
   structured projection error and do not write a partially valid artifact.
3. Use `build_zirl_canonical_types()` in
   `src/cache/zirl-canonical-types.zith` and
   `build_zirl_hir_code_section()` in `src/cache/zirl-hir-code.zith` for
   canonical types and compact Code. Allocate nested arrays through
   caller-owned storage and propagate their structured errors.
4. Build Metadata from `HirModule.strings`, `types`, `struct_defs`,
   `enum_defs`, and `union_defs`, while retaining caller-provided module name
   and paths. Verify that each type and nominal reference uses the identifier
   space expected by the Metadata codec.
5. Build Attrs from `slot_attrs`, `call_attrs`, `function_attrs`, and
   `safety_attrs.proofs`. Preserve original HIR expression/function/slot
   indices and distinguish required runtime checks from optional proof facts.
6. Build Decls and Templates only from the lossless mappings approved in
   Step 6.2. Keep their source records and HIR-to-section conversion in one
   narrow module; do not make `HirModule` depend on cache record types.
7. Extend `ZirlHirArtifactSource` and `write_zirl_hir_artifact()` in
   `src/cache/zirl-hir-artifact.zith` so callers provide HIR, artifact
   identity, paths, and scratch storage rather than prebuilt section records.
   Keep `write_zirl_artifact()` usable as the generic envelope API.
8. Add a non-empty fixture containing at least one string, primitive type,
   pointer/reference type, function with a parameter and block, declaration,
   and safety-proof record. Add nominal/template cases only when Step 6.2
   supplies a lossless mapping.
9. Assert section IDs/order/schema, writer success, identity counts, and
   deterministic bytes across two writes from equivalent HIR. Mutate source
   HIR after the first projection and confirm output bytes do not alias
   caller-owned HIR slices.
10. Run the exact harness command from Step 6.1.

**Expected output:** Empty and non-empty HIR fixtures produce a complete
five-section envelope; equivalent inputs produce identical bytes and all
fixture assertions pass.

**Failure checks:** If canonical type IDs differ between local HIR and
serialized Code/Metadata/Decls/Templates, stop at the first mismatching
reference and fix the remapping path; do not compare counts alone. If a
section requires data absent from HIR, return to Step 6.2. If storage capacity
is insufficient, return the structured capacity error without exposing a
successful partial result.

**Success criteria:** The complete projection fixture passes, every HIR
reference is valid in its section's identifier space, and the full harness
exits 0.

**Commit:** Commit projection code, focused fixtures, and
`docs/project-files.md` updates as
`feat(cache): project verified HIR into ZIRL sections`.

#### 6.4 Decode typed sections and hydrate a complete HIR artifact

**Goal:** A valid HIR artifact reads into caller-owned storage and reconstructs
an equivalent, verified `HirModule`.

**Sub-steps:**
1. Call `read_zirl_artifact()` in `src/cache/zirl-artifact.zith` to validate
   identity, checksum, section table, and required sections before decoding
   payloads.
2. Decode with `read_metadata_section()` in `src/cache/zirl-metadata.zith`,
   `read_decls_section()` in `src/cache/zirl-decl-section.zith`,
   `read_templates_section()` in `src/cache/zirl-template-section.zith`,
   `read_code_section()` in `src/cache/zirl-code-section.zith`, and
   `read_attrs_section()` in `src/cache/zirl-attrs-section.zith`.
3. Add one typed artifact-read result/storage API to
   `src/cache/zirl-hir-artifact.zith`. Keep decoded arrays and nested slices
   in caller-owned storage. On any failure, return a structured error and
   empty result slices.
4. Reconstruct HIR tables without changing identifier meaning. Reverse
   canonical type remapping for Code, Decls, Templates, and Attrs references;
   preserve function/expression/declaration index identity; restore safety
   proofs separately from required checks.
5. Return artifact identity and paths beside `HirModule`, not inside it.
   Compare identity/path values separately from semantic HIR equality.
6. Add typed comparison helpers to `tests/zirl-hir-artifact.zith` covering
   strings; types, arguments and names; nominal definitions; expression
   variants and payloads; function parameters and blocks; declarations;
   globals; vtables; attributes; and proof operands. Do not compare raw
   struct memory or padding.
7. Add round-trips for empty and non-empty fixtures. Add negative tests for
   truncated payload, checksum mismatch, missing required section, unknown
   required section, incompatible HIR schema, and insufficient decode storage.
8. Run the exact harness command from Step 6.1.

**Expected output:** Valid fixtures read successfully, reconstructed HIR
passes `verify_hir()`, and field-by-field comparison equals the source. Each
malformed/incompatible fixture returns its expected error without exposing
partial HIR.

**Failure checks:** If canonical IDs cannot be reversed uniquely, stop and
revise the canonical-table contract before implementing hydration. If proof
metadata is absent, retain required runtime checks; never infer a proof from
cached code. If the compiler cannot compile the comparator, split it into
typed helpers rather than comparing raw struct bytes.

**Success criteria:** Native tests prove complete round-trip equivalence,
malformed-input rejection, and non-partial failure behavior; the full harness
exits 0.

**Commit:** Commit the typed reader/hydrator, comparator, negative fixtures,
and file-map updates as `feat(cache): restore complete HIR artifacts`.

#### 6.5 Gate derived VIR and codegen on real consumers

**Goal:** self-Zith reuses VIR only when both VIR schema and VM semantic
versions match; it does not claim to have ported absent VIR/codegen modules.

**Sub-steps:**
1. Inspect the source inventory:

   ```sh
   rg --files /home/diogo/self-Zith/src
   ```

   The 2026-10-23 status reports that `src/backends/` and `src/codegen/` do
   not exist. Do not create an unused facade or placeholder.
2. Keep the gates in `src/cache/cache.zith` as the cache-decision contract.
   Add a VIR module only when there is a concrete HIR-to-VIR consumer and a
   runnable fixture; version VIR separately from HIR and container formats.
3. Test matching VIR schema + VM semantic versions, each individual
   mismatch, and invalid zero versions. A mismatch regenerates VIR from
   verified HIR and does not invalidate the HIR artifact.
4. Keep LLVM IR, object files, assembly, and target-specific codegen products
   outside the shared HIR/VIR artifact.
5. Port only codegen input fields required by an actual self-Zith consumer.
   If no consumer exists, record codegen as deferred and do not create
   `src/backends/llvm/codegen.zith`.
6. Run the exact harness command from Step 6.1 and inspect `cache.zith` for
   independent HIR, VIR, and VM semantic version checks.

**Expected output:** Version tests show reuse only when both gates match and
regeneration for every mismatch. The source inventory and file map accurately
state when VIR/codegen modules remain absent.

**Failure checks:** If a VIR version mismatch invalidates HIR, fix that
decision boundary before adding serialization. If no executable VIR consumer
exists, stop after policy tests and report VIR generation as deferred. If
codegen needs frontend-owned data, revise the shared HIR contract instead of
adding a frontend dependency.

**Success criteria:** Version-gate fixtures pass and shared HIR/cache/VIR
interfaces contain no frontend AST, semantic-analysis, or session types.

**Commit:** Commit only tested VIR policy/consumer changes and their tests as
`feat(ir): gate derived VIR reuse by semantic version`. Do not commit empty
placeholder modules.

#### 6.6 Gate frontend interoperability on a real semantic pipeline

**Goal:** Report only the interoperability level that the available
self-Zith frontend can produce and test.

**Sub-steps:**
1. Inspect the frontend inventory:

   ```sh
   rg --files /home/diogo/self-Zith/src/frontend
   ```

2. If the inventory still contains only `src/frontend/lexer.zith` and no
   parser, name resolver, semantic analysis, or proof producer, do not add a
   HIR adapter and do not claim bidirectional interoperability.
3. Once those stages exist, add the narrow adapter that lowers semantically
   checked frontend results to the same shared `HirModule`; keep frontend
   types out of `src/ir/` and `src/cache/`.
4. Add paired fixtures that compile equivalent source constructs through each
   real frontend, compare verified HIR semantically, write an artifact with
   each compiler, and read it with the other compiler in both directions.
5. Run both repositories' documented focused test commands only after
   validating their worktrees and obtaining any required authorization for
   `/home/diogo/Zith`.

**Expected output:** If semantic frontend stages are absent, report
“shared HIR/cache port only; frontend interoperability deferred.” If both
adapters exist, the same fixtures pass in both directions.

**Failure checks:** If a frontend cannot produce required proof/check
metadata, preserve runtime checks and stop the affected fixture; do not
manufacture proof facts. If either cache reader rejects the other's artifact,
report the first section/schema/reference mismatch and do not claim
interoperability.

**Success criteria:** Claim full interoperability only when both frontends
pass common semantic fixtures and bidirectional cache-read tests.

**Commit:** Commit adapter and paired-conformance changes separately from the
shared cache implementation, staging only the relevant frontend, test, and
file-map paths.

#### Step 6 final verification and commit hygiene

**Goal:** Verify and commit each implementation slice without absorbing
unrelated worktree edits.

**Sub-steps:**
1. Before each implementation commit, run
   `git -C /home/diogo/self-Zith status --short` and inspect the exact diff
   with `git -C /home/diogo/self-Zith diff -- <explicit paths>`.
2. Stage only paths named by that slice. Never use `git add -A`,
   `git add .`, `git clean`, or reset/checkout to discard unrelated work.
3. Run
   `ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh
   /home/diogo/self-Zith/tests/run.sh` after each cache/VIR slice.
4. Run
   `/home/diogo/Zith/build/zithc --include
   /home/diogo/self-Zith/src check
   /home/diogo/self-Zith/src/main.zith` only after confirming
   `src/main.zith` and imported files are not blocked by unrelated
   pre-existing syntax changes. If blocked, report that check as blocked;
   do not rewrite unrelated files as part of this work.
5. After each successful slice, inspect `git show --stat --oneline HEAD` and
   confirm the commit contains only that slice's implementation, tests, and
   `docs/project-files.md` updates.

**Expected output:** Each applicable harness run exits 0; the project check
either exits 0 or is explicitly identified as blocked by pre-existing syntax
errors; each commit contains only its declared scope.

**Failure checks:** On any failing test, stop before the next slice, preserve
the failure output, and repair or revise the current slice. If an unrelated
worktree file changes during implementation, leave it untouched and exclude
it from staging.

**Success criteria:** All completed cache/VIR slices have passing focused
tests, every commit is scope-limited, and deferred frontend/codegen work is
explicitly recorded rather than counted complete.

## Final Acceptance Check

1. After sibling implementation Steps 2-5, run the focused CTest command from
   Step 1 and then
   `ctest --test-dir /home/diogo/Zith/build --output-on-failure`. Require every
   non-optional test to pass. Confirm required checks survive absent proof
   metadata, `raw` remains explicitly unchecked, valid proof facts alone can
   justify check elision, unknown required semantics are rejected, legacy
   versions miss cleanly, VIR reuse requires matching versions, and no
   backend-specific output is cross-shared.
2. After the self-Zith port, run
   `ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp sh
   /home/diogo/self-Zith/tests/run.sh`. Run
   `/home/diogo/Zith/build/zithc --include
   /home/diogo/self-Zith/src check
   /home/diogo/self-Zith/src/main.zith` only when unrelated pre-existing
   syntax changes do not block it. Confirm every ported shared module is
   exercised and existing unrelated worktree changes remain untouched.
3. Claim full cross-dialect interoperability only after a real adapter exists
   in each compiler, both adapters pass the same common/safety conformance
   fixtures, and bidirectional cache-read tests pass. If the self-Zith frontend
   still lacks semantic analysis, report the shared-module port as complete
   only to the tested extent and leave cross-dialect interoperability
   explicitly deferred.

If any applicable acceptance check fails, stop and report the exact failing
test and output. Do not weaken compatibility checks to make the suite pass.
