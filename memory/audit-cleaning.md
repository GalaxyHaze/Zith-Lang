# Audit And Cleaning State

This note records the current repo-hygiene audit and the concrete cleaning
steps that can be done without changing compiler behavior. It should be kept
small and updated as each queue item is finished.

## Active WIP (do not touch)

- `src/ir/exec-ir.hpp` and `src/interp/ir-vm.{hpp,cpp}` are untracked WIP for
  the execution IR VM. They are intentionally not committed, not added to the
  build, and not removed by the cleaning pass.
- `docs/plans/abi/execution-ir.md` remains signed around the HIR interpreter.
  The IR VM plan/ADR should be updated only after the WIP compiles and has a
  conforming test.

## Finished In This Pass

- `--interpreted` help/completions now say "HIR interpreter" instead of
  "bytecode path".
- `docs/09-control-flow.md` no longer says literal range forms are not
  implemented.
- `memory/execution-ir-drawing.md` now reflects the signed contract, the WIP
  files, and that the CLI is not wired to the IR VM.
- `CMakeLists.txt` removes `src/interp/ir-vm.cpp` from the source glob so
  reconfigure/build stays green while keeping the WIP files on disk; the same
  exclusion covers `src/ir/hir-to-ir.cpp`.
- `memory/plan-debt-status.md` no longer references the stale `agent5`
  worktree, and `memory/monolith-splits.md` follows the short-note convention
  instead of the old 200-300 line rule.

## Cleaning Queue

Current audit findings that are still open:

- Keep `docs/Zith--.md`, `docs/Zith---implementation.md`, and
  `docs/impl-status.md` as the Zith-- source of truth; reduce duplicated
  status callouts in `docs/` chapters.
- Consolidate or archive the full-Zith spec stack (`Zith-spec.md`,
  `Zith-spec-full.md`) and the full-Zith plan stack so only one active spec
  surface remains.
- Consolidate `memory/` notes that duplicate `docs/plans/` or `docs/specs/`.
- Decide whether `docs/plans/release-artifacts.md` and
  `docs/plans/release-stdlib.md` are active plans or audit notes; archive the
  ones that are only snapshots.
- Update README pipeline text and CLI tables so they do not contradict
  `docs/impl-status.md`.
- Finish the codegen monolith tracking only after the IR VM WIP is resolved,
  to avoid mixing unrelated changes.

## Verification

This pass changed docs and two CLI strings only. Run:

```bash
cmake --build build -j4 --target zithc
ctest --test-dir build -R 'abi-execution|cli-commands' --output-on-failure
```

If the IR VM is later integrated, it must be verified with a real no-LLVM
build and a new ABI-EXEC test before the ADR text is updated.
