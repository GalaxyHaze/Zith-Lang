# Full-Zith Thread Fork/Merge

Full-Zith threads use `fork`, `merge`, and `revoke` as core keywords with
runtime backend objects, not a `Branch` capability or a stdlib-only call
surface. `fork` selects the backend explicitly (`pThread fork Entry(args)`)
and returns that backend's concrete `Thread<T>` handle; `spawn` is a stdlib
shorthand for the active backend. A single `merge` blocks and consumes its
handle, returning the entry result. `merge ... and ...` waits for and consumes
all handles, returning a tuple in operand order. `merge ... or ...` also waits
for and consumes all handles, returning a tagged union for the first thread to
finish. If threads finish simultaneously, the leftmost handle wins. The union
covers all declared result types, including failable states, and does not need
to distinguish handles that return the same type. `revoke x;` removes access
to `x` from every unbounded thread in the statement's scope that holds
revocable access to it. `revoke h1;` revokes every revocable resource passed to
the thread represented by handle `h1`. Both forms prevent new guarded accesses
and wait for active guarded operations to finish. Neither form destroys
resources or terminates a worker, and neither consumes the handle. NRA keeps
`forkCount` as the ownership rule for `share` payloads and treats `detach` as an
opt-in backend method that still leaves the fork pending. The source of truth
is `docs/plans/branch-protocol.md`; Zith-- keeps threads out of core syntax.
