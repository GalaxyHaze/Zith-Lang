# VMG-02: Audit Runtime Parity Against Native

Type: `wayfinder:research` (AFK)
Status: resolved
Research branch: none (report kept in local gap-map notes)
Report: `.scratch/vm-v2-gap-map/research/vm-v2-runtime-parity.md`
Part of: [VM v2 Gap Map](../map.md)

## Question

For programs that both the host-native path and VM v2 accept and execute, what
observable runtime differences exist, and which tests or source evidence
confirm them?

Compare against baseline commit `0c1ae39f`. Focus on observable program
behavior such as output, exit code, values, control flow, memory, and traps.
Exclude external-FFI-dependent behavior and WASM/browser-specific behavior.
Cross-check `docs/Zith--.md`; separate a VM/native mismatch from a possible
native/spec discrepancy. Record diagnostics separately from runtime
differences.

For each finding, report a minimal reproducer or existing test, expected/native
result, VM result, relevant VM operation/runtime location, and any likely
dependency on another gap. Do not change source, tests, or specifications.

## Resolution

No successful paired VM runtime result or confirmed output/exit-code mismatch
was established by the current parity corpus. It reports seven native-success
cases that diverge with VM status 5. The test can assign that status at HIR
lowering, VM lowering, or VM execution, but the verbose allow-list output
prints the static reason rather than the observed phase/detail. Therefore the
seven cases are confirmed native/VM divergences, not seven confirmed
lowering-only failures or runtime mismatches.

The VM-only unit tests establish selected VM semantics but do not compare
those programs with native. The parity harness also compares only output and
exit code, not VM status, which is a coverage risk for a future paired corpus.
No native/spec discrepancy was established.

Evidence and limitations: [runtime parity research](../research/vm-v2-runtime-parity.md).
