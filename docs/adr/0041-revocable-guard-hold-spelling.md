# Revocable Guard `hold()` Spelling

Status: accepted as a full-Zith design direction. The runtime is not
implemented and Zith-- is unchanged.

The scoped guard returned by a revocable resource uses `.hold()` rather than
`.acquire()`. Holding the guard keeps the revocable access valid for the
guard's scope and allows ordinary access without repeating revocation checks.
The guard cannot escape that scope. This is a lifetime/access operation, not a
synchronization operation. The spelling supersedes `.acquire()` in
[ADR-0026](0026-nra-aggregate-nodes-and-revokable-flows.md) without changing
its runtime contract. `.sync()` remains the synchronization-oriented API.
