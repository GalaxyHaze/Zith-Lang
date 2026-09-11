# {AGENT}: {TASK}

## Goal

Close the nominal type debt for `type Name = T`: either make the nominal
construction/field-access surface explicit for Zith--, or document the current
`as` cast surface as the intended Zith-- contract.

## Context

- Plan: debt, nominal `type` surface.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "type Name = T é nominal mas sem
  sintaxe completa", and `docs/impl-status.md` under `Types`.
- `type Name = T` currently creates a nominal one-field wrapper that is not
  interchangeable with `T`. `as` wraps/unwraps the wrapper in sema and lowers
  it in HIR.
- The debt says the decision is open: define explicit construction/access
  syntax, or declare that the current cast-based path is sufficient for Zith--.

## Scope

1. Decide the Zith-- contract for `type Name = T` and record it in
   `docs/implementation-debt.md`, `docs/impl-status.md`, and
   `docs/03-type-system.md` without changing unrelated language semantics.
2. The primary deliverable is the decision, not new syntax. Do not implement a
   new construction/access syntax in this wave; if the decision requires one,
   record the follow-up in the debt note and keep this task green.
3. Add focused tests that lock the exact accepted and rejected forms of the
   current cast-based surface for construction, extraction, and type identity.
4. Keep `alias` transparent behavior untouched and document that split.

Owned files for implementation/refactor, when changed for this debt:

- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`,
  `docs/03-type-system.md`, and the memory note that documents the decision if
  one is created.
- Tests: only `tests/test-nominal-type-debt.cpp`; do not edit existing test
  executables. Register the new `add_zith_test` line in `CMakeLists.txt`
  without reordering existing lines.
- Shared docs: edit only the section/line for this item in each file and never
  revert changes that are not yours.

Out of scope:

- All compiler source files in this wave. This task does not implement parser,
  sema, HIR, codegen, cache, or resolver changes.
- Numeric narrowing diagnostics; `src/sema/sema-cast-coerce.cpp` belongs to
  another debt task.
- Opaque registry/canonicalization and trait/interface import conformance; the
  cache/resolution files belong to other debt tasks.

## Acceptance Criteria

- Zith-- nominal types have one explicit documented contract.
- Accepted/rejected nominal forms are covered by deterministic tests.
- No `alias` behavior or unrelated cast behavior regresses.
- The debt entry is no longer marked as an open decision unless the chosen
  scope deliberately leaves a documented follow-up.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'nominal|modern-types|codegen|frontend' --output-on-failure
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
