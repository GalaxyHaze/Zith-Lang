# Zith Project Roadmap (Sketch)

Status: sketch; no calendar dates are assigned. This roadmap sets direction and
exit gates. Each implementation issue derived from it must name the exact files,
commands, test fixtures, expected output, and recovery steps before execution.
Execution preconditions: preserve existing worktree changes; verify the actual
bootstrap toolchain and commands; inventory the source files before migration.
This roadmap itself changes no implementation source.

## Objective

Develop the full Zith language and compiler in this repository. Use the current
`Zith--` toolchain and C where needed as bootstrap substrates, then migrate
project-owned source incrementally to full-Zith syntax and Zith implementations
as compiler support and tests make each migration safe.

## Working Rules

- Treat full Zith as the target; reconcile imported design documents before
  promoting individual rules into the local specification.
- Keep a known-good bootstrap compiler available throughout source migration.
- Implement and test a language capability before using it to rewrite compiler
  or project source.
- Migrate syntax and C implementation in small, dependency-ordered slices.
- Preserve behavior with external tests before and after each migration.
- Retain C only at an explicitly documented platform or ABI boundary while it
  remains necessary; review that exception as full-Zith capabilities grow.
- Do not assign dates or claim cross-target support until the host bootstrap is
  reproducible.

## Milestones

### 1. Reconcile the project baseline

**Goal:** Establish one locally understood full-Zith design baseline and make
the current implementation boundaries explicit.

**Work:**
- Reconcile the imported specification, ADRs, and notes; preserve provenance;
  record unresolved design questions.
- Resolve the parser gap in `docs/architecture.md` and the `os/` discrepancy
  between that document and ADR-0011.
- Inventory project-owned sources by language, generated status, dependencies,
  and existing tests.
- Verify and record the bootstrap build and test commands. The current tool
  exposes `zithc check`, `zithc build`, and `zithc test`; confirm which apply to
  this repository rather than assuming sibling-project commands do.

**Exit gate:** A local specification identifies accepted full-Zith contracts;
known disagreements are listed rather than silently blended; every
project-owned source file is classified; the bootstrap build and test commands
are reproducible.

**Stop condition:** If imported material contradicts an accepted local ADR or
has unclear provenance, do not implement the disputed behavior until it is
resolved in the specification or a new ADR.

### 2. Establish the bootstrap compiler spine

**Goal:** Build and test a small compiler path in this repository using the
available `Zith--` toolchain and any C substrate confirmed by the inventory.

**Work:**
- Define the external compiler seam and structured diagnostics.
- Build the context and the minimum frontend needed for one small valid program
  and one small invalid program.
- Connect a thin CLI driver.
- Add repeatable tests at the compiler seam and record exact invocation and
  expected output in the implementation issue.

**Exit gate:** A clean checkout can build the compiler, compile and execute the
chosen minimal program, and report a stable diagnostic for the invalid
program.

**Stop condition:** If a clean build depends on undocumented state in the
sibling checkout or host, record and remove that dependency before starting
language-source migration.

### 3. Grow full-Zith support by vertical slices

**Goal:** Make compiler support, tests, and usable full-Zith examples advance
together.

**Work:**
- Select features in dependency order from the reconciled specification.
- Implement each feature in the current bootstrap syntax and C where necessary.
- Add parser, semantic, proof, and backend coverage appropriate to the feature.
- Add an end-to-end example demonstrating observable behavior.

**Exit gate:** Every feature marked implemented has compiler tests, a
user-visible acceptance test, and a documented unsupported boundary for
adjacent features.

**Stop condition:** A feature does not enter the source-migration queue while
its end-to-end test fails or its specification remains contradictory.

### 4. Migrate project source from legacy syntax

**Goal:** Replace project-owned `Zith--` source with full-Zith syntax in small,
verifiable increments.

**Work:**
- Order modules by dependency, starting with leaves.
- Capture current behavior for the module before conversion.
- Confirm the bootstrap compiler supports every new construct in the module.
- Convert only that module, then run its focused tests and the repository suite.

**Exit gate:** Each converted module builds through the documented bootstrap
chain and has no unintended legacy syntax; remaining legacy files are tracked
in the source inventory.

**Stop condition:** If conversion changes behavior or requires an unsupported
language feature, stop that slice, implement the missing feature, and retry
without reverting unrelated work.

### 5. Migrate project-owned C implementations

**Goal:** Replace C implementation code with Zith where the language and
runtime can express the same tested behavior.

**Work:**
- Keep stable interfaces while replacing one C component at a time.
- Add parity tests for the old and new implementations.
- Record each remaining C platform or ABI boundary with its reason and review
  condition.

**Exit gate:** No project-owned C implementation remains outside the documented
exception list; every migrated component passes parity and integration tests.

**Stop condition:** Keep the C implementation if the Zith replacement cannot
meet the same contract or if the platform/ABI boundary is not yet understood.
Do not hide the gap behind an untested adapter.

### 6. Close the self-hosting bootstrap

**Goal:** Use a compiler built from the project to rebuild and verify the next
compiler generation from full-Zith project sources.

**Work:**
- Build stage 1 with the established bootstrap compiler.
- Build stage 2 with stage 1.
- Run the same compiler and language test suites on both generations.
- Record artifact differences and explain any nondeterminism.

**Exit gate:** A clean checkout can reproduce the stage0-to-stage1-to-stage2
chain, and stage 1 and stage 2 pass the agreed compiler and language suites.
Retire a legacy path only after no maintained source depends on it.

**Stop condition:** If stage 1 cannot compile the sources used to build it,
keep the last working bootstrap compiler and fix the missing language/compiler
capability before removing legacy support.

### 7. Expand targets and distribution

**Goal:** Add target support without coupling language stages to individual
operating systems.

**Work:**
- After host bootstrap closure, execute the target sequence in
  [`cross-compile.md`](cross-compile.md).
- Revisit the OS-C boundary and macOS distribution proposal against the
  migrated Zith implementation before publishing target support.
- Add one reproducible build and runtime or cross-execution test per advertised
  target.

**Exit gate:** Each advertised target has a reproducible build command and a
runtime or cross-execution test; distribution claims match tested artifacts
and reviewed platform constraints.

**Stop condition:** Do not advertise a target based only on successful linking;
require its specified execution test or document the exact limitation.

## Final Acceptance

The roadmap is complete when full-Zith behavior is specified locally, the
compiler can bootstrap through two consecutive generations, project-owned
legacy syntax and C code are either migrated or listed as explicit reviewed
exceptions, and every advertised target has a reproducible test.
