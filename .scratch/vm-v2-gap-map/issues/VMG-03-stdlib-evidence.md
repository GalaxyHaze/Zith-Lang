# VMG-03: Reconcile Stdlib Parity Evidence and Documented Gaps

Type: `wayfinder:research` (AFK)
Status: resolved
Research branch: none (report kept in local gap-map notes)
Report: `.scratch/vm-v2-gap-map/research/vm-v2-stdlib-evidence.md`
Part of: [VM v2 Gap Map](../map.md)

## Question

At baseline commit `0c1ae39f`, which stdlib-dependent programs are accepted by
the native path but rejected or observably different under VM v2, and do the
existing parity table, implementation-debt notes, and tests accurately
describe that behavior?

Audit the current parity cases and relevant stdlib/test coverage. Include
non-external-FFI behavior; exclude FFI-only cases and WASM/browser behavior.
Distinguish VM lowering rejection from runtime mismatch, record reproducible
evidence and relevant source/test locations, and flag possible conflicts with
`docs/Zith--.md` rather than assuming the native result is correct. Identify
important native-supported stdlib examples that are not represented in the
current parity slice, without implementing new tests.

Do not change source, tests, or specifications.

## Resolution

The baseline parity run reports status 5 for all seven stdlib cases while the
native path succeeds. The harness allow-lists those outcomes, so the passing
CTest result does not demonstrate runtime parity. The parity document
misclassifies the three literal console cases as passing and omits them from
its status-5 table. The exact phase and per-case cause remain unproven because
the harness collapses failures from multiple stages into the same status.

The report also identifies useful missing native-only parity examples,
including dynamic arrays, broader allocator behavior, formatting variants,
and controlled stdin. These are inventory candidates, not confirmed VM gaps.
No native/spec discrepancy was established.

Evidence and limitations: [stdlib parity research](../research/vm-v2-stdlib-evidence.md).
