# {AGENT}: {TASK}

## Goal

Remove the repeated `ProjectConfig + Options` field concatenation with one
arena-backed merge helper and deterministic ordering.

## Context

- Plan: debt, concatenação ProjectConfig + Options.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md`, section "Concatenação ProjectConfig +
  Options".
- `CompilationSession` and `native-link.cpp` duplicate loops over
  `mProjectConfig.*` followed by `mOpts.get().*` for include dirs, C source
  dirs, defines, library dirs, and libraries.

## Scope

1. Add one merge helper (recommended name, e.g. `mergeStrings`) that takes the
   project config array, the CLI options array, the arena/vector used at each
   call site, and an append flag.
2. Replace the repeated loops in `src/session/compilation-session.cpp` and
   `src/session/native-link.cpp`.
3. Preserve today's precedence: project config first, CLI options second, and
   optional CLI override/append semantics at each call site exactly as today.
4. Add a focused test that proves the merged result has stable order and that
   an options override still wins where one is expected.

Owned files for implementation:

- Merge helper and call sites: `src/session/compilation-session.cpp`,
  `src/session/native-link.cpp`, `src/session/compilation-session.hpp`,
  `src/session/compiler-driver.hpp` if a declaration must move.
- Config/options headers only if a signature must be tightened:
  `src/cli/project-config.hpp`, `src/cli/options.hpp`.
- Tests: `tests/test-cli-commands.cpp`; do not add new executables.
- Docs: `docs/implementation-debt.md`, editing only the section for this item.

Out of scope:

- Changing TOML parsing or CLI flag definitions.
- Changing merge precedence of unrelated options such as `targetTriple`,
  `sysroot`, gating flags, or output paths.
- Cache/opaque/NRA work; do not edit `src/cache/*`,
  `src/session/persistent-cache.cpp`, `src/sema/nra-facts.*`,
  `src/hir/hir-attrs.hpp`.

## Acceptance Criteria

- Every documented duplicated concatenation is routed through the merge helper
  or explicitly documented as a deliberate exception.
- The resulting include/C-source/define/library orders match current builds.
- No CLI or project config option regresses.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'project|config|cli|native' --output-on-failure
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
