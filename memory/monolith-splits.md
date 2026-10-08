# Monolith Splits Memory

Short operational pointer for the source-level monolith splits. The execution
contract, the completed-split tables, and the current line counts are owned by
`docs/plans/monolith-splits.md`. This note keeps only the decisions that are
easy to get wrong when editing that plan or `docs/implementation-debt.md`.

## Completed Splits

The frontend-context, compilation-session, codegen-emit, hir-lower-expr, and
frontend-expr monoliths are merged. No source-level candidate is active. The
exact translation units and `wc -l` counts live in
`docs/plans/monolith-splits.md`. Do not restate them here.

## Durable Decisions

- New translation units are named for the extracted responsibility, never a
  generic `utils` or `helpers` file.
- The splits are behavior-preserving: no diagnostic string, error code, public
  API, or internal symbol name changes unless the refactor exposes a real
  duplicate helper.
- Re-run CMake after adding a `.cpp` because `CMakeLists.txt` globs the source
  list.
- Record exact `wc -l` output, never approximate counts. The historical
  ~1789 and ~1985 numbers are markers only and must not be quoted as current.
- Use absolute paths in links when the repo has worktrees.

## Stale References

- `docs/plans/platform-imports.md` moved to
  `docs/plans/archive/platform-imports.old.md`. It is no longer active.
- The platform-import resolver now lives in
  `src/session/frontend-module-analysis.cpp` after the frontend-context split.
- `docs/implementation-debt.md` must not list the session files as pending with
  old large line counts, and C interop is `Working (validated C)`, not
  `common C`.

## Hygiene

This is a Zith infrastructure track, independent of the Zith vs `Zith--`
feature split. Do not confuse it with debt entries that reference the same
files: `compilation-session.cpp` and `hir-lower-expr.cpp` own some diagnostics,
and the C struct-by-value debt is a validated-surface question, not a split
candidate. When editing, search for stale markers first:

```bash
rg -n "~1789|~1985|~2140|platform-imports\.md" docs memory
```
