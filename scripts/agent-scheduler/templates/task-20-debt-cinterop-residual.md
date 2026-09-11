# {AGENT}: {TASK}

## Goal

Reduce the residual C record ABI gap to one concrete, repo-verifiable slice:
more validated record forms with explicit skipped diagnostics, or a documented
decision that a particular class remains intentionally unimported.

## Context

- Plan: debt, C interop is `Working (validated C)`, not full ABI.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "C interop é Working (validated
  C), não ABI completa".
- `src/cinterop/c-header.cpp` currently validates record layout, skips
  bitfields/packed/anonymous/flexible/edge-case scalar records, and records
  explicit skips.

## Scope

1. Pick one small record form that can be validated safely for x86-64 and
   AArch64 Linux. Recommended candidates: records with multiple adjacent
   i32/i64 fields where Clang layout is proven, or pointer fields in otherwise
   fixed records.
2. Extend `lowerRecordLayout` only as far as this form can be proven; keep the
   explicit skip behavior for anything not proven.
3. Add by-value parameter and result tests for both target triples, including
   the nested-record form if it becomes supported.
4. Record the remaining unsupported forms and the reasoning in the debt note.

Owned files for implementation:

- C binder: `src/cinterop/c-header.cpp`, `src/cinterop/c-header.hpp`.
- Tests: `tests/test-cinterop.cpp`.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`.
  Edit only the section/line for this item in each shared file.

Out of scope:

- Full libclang macro/global/string support.
- Function-like macros or flexible-array-by-value ABI.
- C compilation or linker work in `src/cc/`, `src/cli/`, and
  `src/session/native-link.cpp`; this task does not change the native link
  command construction.
- Opaque registry work; do not touch `src/session/persistent-cache.cpp` or
  the hydration logic in `src/session/compilation-session.cpp`.
- Cache compact layout; this wave does not own `src/cache/*`.

## Acceptance Criteria

- Each chosen record form passes as parameter and result on the two supported
  Linux triples.
- Every form still not validated produces an explicit skip in the test data.
- No existing scalar/pointer/nested simple-record behavior regresses.
- Debt text lists exactly which ABI forms remain unimplemented and why.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'cinterop|c-compile|abi' --output-on-failure
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
