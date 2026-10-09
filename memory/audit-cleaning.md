# Audit And Cleaning State

This note records the current repo-hygiene audit and the concrete cleaning
steps that can be done without changing compiler behavior. It should be kept
small and updated as each queue item is finished.

## Active WIP (do not touch)

- `docs/plans/abi/execution-ir.md` remains signed around the HIR interpreter.
  The archived execution-IR slice (`archive/execution-ir-v1/`) is not in the
  active tree; keep the plan/ADR aligned with the committed slice state.

## Finished In This Pass

- `--interpreted` help/completions now say "HIR interpreter" instead of
  "bytecode path".
- `docs/09-control-flow.md` no longer says literal range forms are not
  implemented.
- `memory/execution-ir-drawing.md` now reflects the signed contract and the
  committed IR VM slice.
- `memory/plan-debt-status.md` no longer references the stale `agent5`
  worktree, and `memory/monolith-splits.md` follows the short-note convention
  instead of the old 200-300 line rule.
- Chapter docs no longer restate feature status. Each numbered chapter points at
  `docs/impl-status.md` (#58).
- The full-Zith spec stack is consolidated to `docs/Zith-spec.md`, and the aggregate
  `Zith-spec-full.md` is archived (#59).
- `memory/` notes that duplicated plans or specs now point at the owner and
  keep only unique facts, and `memory/README.md` reflects the set (#60).
- The two release plans are archived audit snapshots, and the planning index
  points at the archived locations (#61).
- The README pipeline and CLI tables now match `docs/impl-status.md` (#62).
- The native no-LLVM execution branch is removed (#56). `run.cpp` no longer has
  `useIrVm`; VM v2 is the WASM runtime and `--interpreted` is the HIR
  interpreter. `src/vm/` is optional via `ZITH_BUILD_VM`, `--emit-vir` errors
  when the slice is absent, and `test-vm-v2` skips with exit code 77.
- The doc invariants are guarded by `scripts/check-docs-consistency.py` (#65),
  registered as the `docs-consistency` CTest and a CI step. It fails when a
  chapter doc repeats a feature-status label or when the README pipeline/CLI
  tables drift from `docs/impl-status.md`. The status doc gained the missing
  `zithc completion <shell>` row it was drifting on.

## Cleaning Queue

Current audit findings that are still open:

- Harden the `opaque` canonical tag rule so canonization and lowering share one
  rule (tracked in #57).

## Verification

This pass changed docs and two CLI strings only. Run:

```bash
cmake --build build -j4 --target zithc
ctest --test-dir build -R 'abi-execution|cli-commands' --output-on-failure
```

If the IR VM is later integrated, it must be verified with a real no-LLVM
build and a new ABI-EXEC test before the ADR text is updated.
