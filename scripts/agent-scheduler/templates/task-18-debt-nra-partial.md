# {AGENT}: {TASK}

## Goal

Make a concrete, small NRA/ownership slice deterministic and documented, rather
than re-attempting the full alive/dead/lent proof machine.

## Context

- Plan: debt, partial NRA and stable pre-HIR boundary.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "NRA está parcial";
  `docs/impl-status.md` under `NRA / Reference Analysis`; and
  `memory/nra-hir-boundary.md` as the operational contract.
- NRA currently accumulates residual local/call/narrowing facts before HIR and
  lowers them into `HirAttrs` side tables only. It is not the full four-rule
  proof machine.

## Scope

1. Pick and implement exactly one missing NRA/ownership slice that can be
   proven end-to-end for Zith--. Recommended candidate: model a missing
   use-after-move or double-borrow case with ownership diagnostics and residual
   facts.
2. Keep the stable pipeline order `sema -> solve -> nra -> lower -> codegen`
   and do not introduce HIR ownership nodes.
3. Add focused negative and positive tests proving accepted and rejected
   programs, and keep no-ownership programs producing empty residual attrs.
4. Update the debt and the playbook to record exactly which slice is done and
   what remains.

Owned files for implementation:

- NRA accumulator: `src/sema/nra-facts.cpp`, `src/sema/nra-facts.hpp`.
- HIR boundary side tables: `src/hir/hir-attrs.hpp`.
- Sema ownership/narrowing helpers used by this slice:
  `src/sema/narrowing.cpp`, `src/sema/narrowing.hpp`, `src/sema/sema-assign.cpp`.
- Tests: `tests/test-memory-qualifiers.cpp`; do not edit
  `tests/test-hir-lower-modern.cpp` or add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`,
  `memory/nra-hir-boundary.md`.
  Edit only the section/line for this item in each shared file.

Out of scope:

- Implementing all of the alive/dead/lent proof machine and every future
  ownership diagnostic.
- Moving ownership proof behind final lowering.
- Editing pipeline orchestration in `src/session/compilation-session.cpp`; the
  required `nraStage()` already exists and the project-config merge task owns
  that file in this wave.
- Formatter behavior; do not edit `src/formatter/*`.
- Trait/interface import conformance; do not edit
  `src/session/frontend-symbol-resolution.cpp` or `src/sema/sema-method.cpp`.

## Acceptance Criteria

- The selected slice has a deterministic accepted/rejected test pair.
- No ownership fact is invented in HIR; residual facts stay in side tables.
- Existing ownership tests pass unchanged.
- The active debt text lists the completed slice as resolved and the remaining
  larger scope as open.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'memory|hir-lower|nra' --output-on-failure
cmake --build {REPO}/build --target fmt-check
ctest --test-dir {REPO}/build --output-on-failure
```

After success:

```bash
cd {REPO}/.awt/{AGENT}
{AWT_SKILL_DIR}/scripts/awt checkin {REPO} "{AGENT}: {TASK}"
{AWT_SKILL_DIR}/scripts/awt request-merge {REPO} "{AGENT}: {TASK}"
```

## End Of Task

After requesting merge, do NOT finish this session. Return to the listening
loop: re-read `TASK.md` whenever the scheduler advances this worktree. Execute
each new task as it appears and request merge again after completing it. Stop
only when `TASK.md` contains a `# Status` section with `end` below it.
