# VM v2 Gap Map

## Destination

Produce a reviewed, evidence-backed inventory of functional gaps and scoped
candidates between VM v2 and the host-native compiler path for native-accepted
programs that do not depend on external FFI. This map remains the evidence
inventory. The follow-up specification draft is recorded separately in
[spec.md](spec.md). Neither document implements changes.

## Notes

- Baseline: commit `0c1ae39f` (`chore: checkpoint compiler and stdlib updates`).
- Compare the host-native LLVM path with VM v2 on the same program.
- Include native-accepted behavior that VM v2 rejects during lowering and
  observable runtime differences for programs that both paths run.
- Use `docs/Zith--.md` as a secondary correctness check. If native and VM
  disagree with the spec, record that as a possible native-path discrepancy,
  not automatically as a VM gap.
- Exclude external-FFI-only programs and do not expand FFI. Keep stdlib cases
  that use validated VM runtime externs visible but label them
  boundary-dependent rather than silently treating them as core VM behavior.
- Record evidence for each finding, preferably a source case or test that can
  reproduce it. Record compile diagnostics separately from runtime behavior.
- Prioritize observable correctness differences first, then gaps that unlock
  broader native-supported behavior. Retain dependency and breadth information
  as separate dimensions. Do not estimate implementation effort yet.
- External FFI-dependent cases, FFI mode selection, and FFI expansion are out
  of scope. A future selectable external-FFI/sandbox mode is not designed here.
- WASM/browser behavior and performance comparisons are out of scope.
- Do not change compiler, language, stdlib, or test behavior during this map.
- Baseline verification: `cmake --build build -j 2` passed. CTest passed 40/42
  targets. `test-vm-v2` and `test-codegen` failed, while `test-stdlib-parity`
  passed. Treat those results as evidence to classify, not as fixes authorized
  by this map.

## Decisions so far

- On 2026-10-18, the reviewed parity scope was agreed: optionals, numeric
  enums, tagged and raw unions, `dyn`, bare `opaque`, runtime pipelines, and
  state-machine transfers. State-machine VM behavior remains unverified and
  must not be described as an observed runtime gap.
- Numeric enums preserve current behavior when no numeric type is declared and
  may specify a numeric type. `enum: union` is outside Zith--.
- Optional parity includes the specified absent-value result of `must`.
  General runtime-panic terminators remain outside this scope.
- `raw opaque`, allocator/`InPlace` contracts, and external-FFI-dependent
  behavior are not acceptance requirements. FFI mode selection and expansion
  remain undecided future work.
- The new scope decision is recorded in
  [ADR-0042](../../docs/adr/0042-vm-v2-zith-parity-scope.md), which changes
  only the conflicting future-scope statements in ADR-0021.
- [Map VM v2 lowering coverage against native](issues/VMG-01-lowering-coverage.md):
  source-backed gaps cluster around optional, enum/union, `dyn`, `opaque`, and
  pipeline HIR support. State-machine transfers are now in scope, but their VM
  behavior remains unmeasured. General runtime-panic terminators remain out of
  scope.
- [Reconcile stdlib parity evidence and documented gaps](issues/VMG-03-stdlib-evidence.md):
  the baseline parity test allow-lists seven native-success/VM-status-5
  divergences. The current parity document mislabels three console cases as
  passing. Per-case failure phases remain unverified.
- [Audit runtime parity against native](issues/VMG-02-runtime-parity.md):
  no runtime mismatch is confirmed. The existing parity slice does not produce
  a successful paired VM result, and it does not expose the failure phase for
  status-5 cases.

## Evidence Gaps Retained

The draft specification includes state-machine transfers in its proposed
scope, but the inventory does not yet establish their VM outcome. Gather
paired evidence before claiming a specific VM behavior or implementation
acceptance result.

- The exact failure phase for each status-5 parity case, since the current
  harness prints the allow-list reason rather than the observed detail.
- Behavior-level outcomes for state-machine transfers.
- Whether validated VM runtime externs and allocator operations belong in the
  eventual sandbox parity baseline. They remain explicitly conditional here.
- Outcomes for general runtime-panic terminators, global constants, cleanup,
  and other candidates remain outside this scope.

## Out of scope

- Implementing VM, stdlib, or compiler fixes.
- Treating this inventory as an implementation plan. The separate draft
  specification records the proposed behavior contract.
- Designing or implementing external-FFI versus sandbox mode selection.
- Expanding external FFI support or auditing FFI-only behavior as VM parity.
- `enum: union`, `raw opaque`, general runtime-panic terminators, global
  constants, and cleanup.
- Comptime pipelines and the full variadic-slice surface.
- WASM/browser VM parity.
- Performance benchmarking or optimization.

## Local Tracker

See [the ticket index](issues/README.md). Local ticket IDs use the `VMG-NN`
prefix. Research report locations and status are recorded on each ticket.
