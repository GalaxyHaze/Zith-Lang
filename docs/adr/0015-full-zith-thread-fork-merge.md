# Full-Zith Thread Fork/Merge

Full-Zith threads use `fork`/`merge` as core keywords with runtime backend
objects, not a `Branch` capability or a stdlib-only call surface. `fork`
selects the backend explicitly (`pThread fork Entry(args)`) and returns that
backend's concrete `Thread<T>` handle; `spawn` is a stdlib shorthand for the
active backend; `merge` blocks, consumes the handle once, and returns exactly
the entry result type. NRA keeps `forkCount` as the ownership rule for `share`
payloads and treats `detach` as an opt-in backend method that still leaves the
fork pending. The source of truth is `docs/plans/branch-protocol.md`; Zith--
keeps threads out of core syntax.
