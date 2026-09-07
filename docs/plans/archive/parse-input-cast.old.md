# Archived: ParseInput and InputLine.cast<T>

> Status: implemented in Zith--. This step is archived because the work no
> longer belongs in active plans. The contract and residual `*char` decision
> are recorded in `docs/implementation-debt.md` and `docs/impl-status.md`.

The original step planned `InputLine.cast<T>` backed by a `ParseInput` trait
with `fn parse(self: view InputLine): ?Self`. Zith-- now ships primitive
implementations for `bool`, `f32`, `f64`, `i32`, and `u32`. `*char` remains
intentionally out of scope; strings stay available through the `text()`
adapter.

No active implementation steps remain for this document. Parsing additional
primitives is an optional extension to the existing `ParseInput` impls and is
tracked as non-debt unless product changes the contract.
