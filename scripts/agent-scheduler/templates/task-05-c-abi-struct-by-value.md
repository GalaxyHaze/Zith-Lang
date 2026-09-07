# {AGENT}: {TASK}

## Goal

Add a validated simple-record C struct-by-value ABI to the Zith-- C binder,
or reject records whose ABI cannot be proven.

## Context

- Plan: C ABI struct-by-value fix.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/adr/0009-validated-c-abi-surface.md` and the ABI step in
  `docs/plans/standalone-c-toolchain.md`.
- Current state: common C declarations import; struct parameters/results import
  as named foreign types, but struct-by-value ABI is not verified.
- Target-specific layout must live under `src/cinterop/abi/` or another
  target-aware module, never global hardcoded sizes.

## Scope

1. Add target triple and sysroot awareness to `CHeaderParserOptions` or the
   existing equivalent.
2. Implement record layout for simple records: scalar fields, plain pointers,
  and nested validated records.
3. Reject bitfields, explicit packing, anonymous records, flexible arrays, and
  `long double`/`__int128` edge cases with clear diagnostics.
4. Add ABI metadata so lowering rejects any record value call lacking a proven
  layout/alignment path.
5. Add positive and negative tests for `x86_64-linux-gnu` and
  `aarch64-linux-gnu` covering by-value struct parameters and returns.
6. Update `docs/18-c-interop.md` with the exact supported record subset and
  update implementation debt when the C interop item moves to done.

Out of scope:

- Bitfields, packed/anonymous records, flexible arrays, globals, strings, and
  function-like macros.
- Changing behavior for scalar/pointer C declarations.
- Full standalone toolchain migration.

## Acceptance Criteria

- No struct-by-value case is accepted without a proven layout/alignment path.
- Unsupported records produce explicit diagnostics instead of silent skipping.
- Positive simple-struct tests pass on the configured target.
- C interop tests and full CTest pass.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'c-binder|codegen' --output-on-failure
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
