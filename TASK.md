# agent4: Validated simple-record C struct-by-value ABI

## Goal

Add a validated simple-record C struct-by-value ABI to the Zith-- C binder,
or reject records whose ABI cannot be proven.

## Context

- Plan: C ABI struct-by-value fix.
- Worktree: /home/diogo/Zith/.awt/agent4 (branch awt/agent4).
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
cmake --build /home/diogo/Zith/build -j4
ctest --test-dir /home/diogo/Zith/build -R 'c-binder|codegen' --output-on-failure
cmake --build /home/diogo/Zith/build --target fmt-check
ctest --test-dir /home/diogo/Zith/build --output-on-failure
```

After success:

```bash
cd /home/diogo/Zith/.awt/agent4
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt checkin /home/diogo/Zith "agent4: Validated simple-record C struct-by-value ABI"
/home/diogo/.byteask/skills/agent-worktrees/scripts/awt request-merge /home/diogo/Zith "agent4: Validated simple-record C struct-by-value ABI"
```

## End Of Task

When complete, request merge and wait for the scheduler to advance or write
a `# Status` marker with `end` below it.
