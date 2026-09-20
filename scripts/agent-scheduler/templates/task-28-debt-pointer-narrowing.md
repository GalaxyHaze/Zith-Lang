# {AGENT}: {TASK}

## Goal

Replace the temporary unchecked `?*T -> *T` acceptance with flow-sensitive
pointer narrowing: `is null` must prove non-null for pointer deref, arrow,
index, and coercions that expect `*T`. Emit `E3005 NullDerefUnproven` for
unproven use instead of silently accepting it.

## Context

- Plan: debt, unchecked nullable-pointer coercion and missing pointer narrowing.
- Worktree: {REPO}/.awt/{AGENT} (branch awt/{AGENT}).
- Follow `docs/implementation-debt.md` section 6 ("Outras incompletudes
  registadas") and `docs/impl-status.md` under `Types` and `Known Debt`.
- `PerModuleSema::allowsUncheckedNullablePointer` in
  `src/sema/sema-zith.cpp` is the single temporary removal point documented in
  `src/sema/sema-zith.cpp` and `docs/08-error-handling.md`.
- `inferArrow`, `inferIndex`, and casts through `sema-cast-coerce.cpp` also
  currently accept a nullable pointer without proof.

## Scope

1. Define when a `?*T` value is narrowed to a non-null `*T`:
   `x is null` in the false branch, `not (x is null)` in the true branch, and
   any existing optional-payload narrowing path already used for `?T`.
2. Keep aggregate optional narrowing (`?T` with non-pointer payload) working.
3. Emit `E3005` for `?*T` uses that require `*T` without a proven non-null
   branch. The diagnostic must point at the offending expression and include
   enough context to suggest `is null`/`not (is null)`.
4. Keep C-interop ergonomics intact where possible while removing the unchecked
   escape hatch. If `must`/`raw` is the correct opt-out for unsafe reads, reuse
   those existing operators instead of adding syntax.
5. Cover deref (`*p`), arrow (`p->field`), index (`p[i]`), and type coercion
   (`?*T` passed to `*T`) in tests.

Owned files for implementation:

- Sema: `src/sema/sema-index.cpp`, `src/sema/sema-zith.cpp`,
  `src/sema/sema-expr.cpp`, `src/sema/sema-modern.hpp`.
- Cast/coercion path only enough to call the new proof when converting
  `?*T -> *T`: `src/sema/sema-cast-coerce.cpp`. Do not broaden the existing
  narrow-cast overflow work.
- Tests: `tests/test-codegen.cpp`, `tests/test-frontend.cpp`,
  `tests/test-optional-slice.cpp`; do not add new executables.
- Docs: `docs/implementation-debt.md`, `docs/impl-status.md`,
  `docs/08-error-handling.md`. Edit only the section/line for this item in each
  shared file.

Out of scope:

- Pointer narrowing after `is <Type>` on opaque/tagged unions.
- Numeric narrowing overflow; keep the existing checks intact.
- Range and loop ownership; do not touch `src/sema/hir-lower-block.cpp`.
- Export namespace resolution and `src/session/*` in this wave.
- Adding a new pointer-safety keyword or attribute.

## Acceptance Criteria

- `?*T` passes to `*T` after a proven `not (x is null)` branch.
- `?*T` deref/arrow/index outside a proven branch reports `E3005`.
- `?T` aggregate payload narrowing still lowers correctly.
- `must`/`raw` behavior, C import signatures, and existing pointer tests remain
  green where the language contract did not change.
- `E3005` is no longer registered-but-never-emitted.

## Verification

```bash
cmake --build {REPO}/build -j4
ctest --test-dir {REPO}/build -R 'pointer|optional|codegen|sema|index|cast' --output-on-failure
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
