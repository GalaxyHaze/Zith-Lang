# VMG-01: Map VM v2 Lowering Coverage Against Native

Type: `wayfinder:research` (AFK)
Status: resolved
Research branch: none (report kept in local gap-map notes)
Report: `.scratch/vm-v2-gap-map/research/vm-v2-lowering-coverage.md`
Part of: [VM v2 Gap Map](../map.md)

## Question

Which programs and language constructs accepted by the host-native compiler
does VM v2 reject during HIR-to-VM lowering, and what evidence, diagnostics,
and shared causes establish each gap?

Compare against baseline commit `0c1ae39f`. Cover the active VM v2 path, not
archived interpreter/IR implementations. Include native-accepted behavior
even when VM v2 currently rejects it. Exclude programs whose behavior depends
on external FFI and exclude WASM/browser-specific behavior. Cross-check
`docs/Zith--.md`; flag possible native/spec discrepancies separately.

For each finding, report the smallest useful reproducer or existing test,
native result, VM result/diagnostic, relevant lowering/source location, and
whether multiple findings appear to share a root cause. Do not change source,
tests, or specifications.

## Resolution

The source-level inventory identifies missing VM visitor support for optional
construction/extraction, enum values, union operations, `dyn` dispatch, bare
`opaque` operations, and pipeline HIR. These have native runtime or HIR/test
evidence and corresponding VM lowering omissions. They are strong
lowering-gap findings, though most checked-in native runtime reproducers were
not replayed through `--virtual-machine` in this pass.

State-machine tail calls and runtime-panic terminators are separate
correctness-risk candidates: the VM terminator dispatch lacks corresponding
handling, but the resulting runtime behavior was not measured. Global
constant loads, cleanup, canonical type identity, non-literal pointer
indexing, and variadic console formatting remain candidates needing focused
paired evidence. In particular, the current console API passes allocator
before message, while the VM formatting fast path inspects the first call
argument, so the allow-list reason does not prove the formatting failure
cause.

The parity harness reports seven native-success/VM-status-5 divergences, but
does not preserve the observed failure phase/detail in its verbose allow-list
output. Validated runtime extern and allocator cases remain conditional on the
FFI boundary agreed for this inventory. No discrepancy between native
behavior and `docs/Zith--.md` was established.

Evidence and limitations: [lowering coverage research](../research/vm-v2-lowering-coverage.md).
