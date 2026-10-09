# Remove the Native No-LLVM Execution Path

## Status

Accepted.

## Context

`src/cli/cmd/run.cpp` fixes `useIrVm` to `false` when LLVM is present and to
`true` otherwise. The `useIrVm` block lowers the HIR module into VM v2 and runs
it. Every native CI job and every release artifact configures
`ZITH_HAS_LLVM=ON` with `ZITH_REQUIRE_LLVM=ON`, so the native no-LLVM branch is
unreachable in any build the project ships or tests. The branch is still live
on the WASM build, where `ZITH_HAS_LLVM` is not defined.

`test-vm-v2` is registered unconditionally in `CMakeLists.txt` with no check
that `src/vm/` is present. If the VM slice is ever excluded from a build, the
test fails to build instead of being skipped.

The debt record is section D of `docs/implementation-debt.md`. It was triaged
as a product decision, not a bug: either the no-LLVM path gets a dedicated CI
job, or the path is removed.

## Decision

The native no-LLVM execution branch is removed. `useIrVm` no longer exists for
native builds. VM v2 stays the execution path only where it actually runs, the
WASM build, and the `--interpreted` flag stays exclusively on the HIR
interpreter.

`test-vm-v2` gains a build-availability guard so it skips when `src/vm/` is not
part of the build instead of failing.

## Considered Options

- Keep the native no-LLVM branch and add a dedicated CI job with
  `ZITH_HAS_LLVM=OFF`. Rejected because the project contract requires LLVM 18+
  for every native artifact, so the job would test a configuration that is
  never released.
- Keep the branch and only add the `test-vm-v2` guard. Rejected because it
  leaves dead code that no configuration can reach.

## Consequences

- `run.cpp` has one native execution path (link and exec) plus the WASM VM v2
  path and the HIR interpreter path.
- Removing the branch makes `src/vm/` optional for native builds, which is why
  the `test-vm-v2` guard is required in the same change.
- Reintroducing a native no-LLVM runtime requires a deliberate spec change and
  a build configuration that actually ships it.
