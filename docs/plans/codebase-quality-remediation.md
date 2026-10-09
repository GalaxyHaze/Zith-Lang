# Codebase Quality Remediation Plan

Status: steps 1-5 DONE (2026-10-07). Findings A, B and C are closed in
`docs/implementation-debt.md`; finding D is triaged into the
`memory/audit-cleaning.md` cleaning queue and remains open by design (it needs a
product decision on the no-LLVM execution path).

Objective: close the four structural/hygiene findings registered in
`docs/implementation-debt.md` under "Dívida de qualidade da code base
(auditoria 2026-10-07)", so the active tree, the docs and the CMake glob stop
contradicting each other, and the remaining coverage gap is explicitly tracked.

Preconditions:

- Working directory: `/home/diogo/Zith`.
- Baseline commit: `935d7c00` (main). Confirm with `git -C /home/diogo/Zith rev-parse --short HEAD`.
- Build tree: `/home/diogo/Zith/build` (already configured; `compile_commands.json` present).
- Tools: `git`, `cmake`, `ctest`, `rg`, `python3` (for `scripts/rag.py`).
- The four findings are described in `docs/implementation-debt.md` sections A, B, C, D.

Files modified by this plan:

- `docs/adr/0024-wasm-vm-v2-abi.md` (step 1)
- `memory/audit-cleaning.md` (steps 1 and 4)
- `memory/agent7-formatter-build.md` (step 1)
- `src/vm/typed-ir.hpp.orig` (step 2, deleted)
- `src/memory/string-interner.hpp`, `src/hir/hir-module.hpp`, `src/cli/options.hpp` (step 3)

Forbidden actions:

- Never `git reset --hard`, `git checkout --`, or `git clean -fdx`.
- Do not touch `docs/nra-spec.md`, the NRA ADRs (0033/0034/0035), or
  `memory/nra-design.md`. Those carry unrelated in-flight edits.
- Do not edit `docs/impl-status.md`; it is verified against compiler behaviour.
- Do not revert the user's uncommitted changes listed by `git status`.
- Do not reformat unrelated code; use `clang-format` only on files this plan edits.

---

### Step 1 - Fix the stale `src/ir` references - DONE

Goal: no tracked document claims that `src/ir/` or `src/interp/ir-vm.*` exist in
the active tree.

Sub-steps:

1. Open `/home/diogo/Zith/docs/adr/0024-wasm-vm-v2-abi.md`.
2. Locate the paragraph beginning "The project also has two execution
   vocabularies in the tree." (around line 13).
3. Replace the sentence "The older `src/ir` + `src/interp/ir-vm.*` execution IR
   is unfinished and is not the default portable runtime; VM v2 is." with:
   "The older execution IR (`exec-ir.hpp`, `hir-to-ir.*`, `ir-vm.*`) is
   archived under `archive/execution-ir-v1/` and is not in the active tree; VM
   v2 is the default portable runtime."
4. Open `/home/diogo/Zith/memory/audit-cleaning.md`.
5. In the "Active WIP (do not touch)" list, remove the first bullet (the one
   naming `src/ir/exec-ir.hpp` and `src/interp/ir-vm.{hpp,cpp}`). Keep the
   second bullet about `docs/plans/abi/execution-ir.md`.
6. In the "Finished In This Pass" list, remove the bullet that says
   "`CMakeLists.txt` removes `src/interp/ir-vm.cpp` and `src/ir/hir-to-ir.cpp`
   from the source glob", because those files no longer exist.
7. Open `/home/diogo/Zith/memory/agent7-formatter-build.md`.
8. Find the sentence naming the excluded `src/ir/hir-to-ir.cpp` and
   `src/interp/ir-vm.cpp` symbols (around line 25).
9. Replace it with: "The archived execution-IR slice lives in
   `archive/execution-ir-v1/` and is not compiled into `zithcLib`."

Command:

    cd /home/diogo/Zith && rg -n "src/ir|ir-vm|hir-to-ir" docs/adr/0024-wasm-vm-v2-abi.md memory/audit-cleaning.md memory/agent7-formatter-build.md

Expected output: after the edits, the command prints only lines that mention
`archive/execution-ir-v1/` or `docs/plans/abi/execution-ir.md`. No line contains
the literal `src/ir/exec-ir.hpp`, `src/interp/ir-vm.cpp`, or
`src/ir/hir-to-ir.cpp`.

Failure checks:

- If `rg` prints `src/ir/exec-ir.hpp`, the first bullet in `audit-cleaning.md`
  was not fully removed. Re-open the file and delete the whole bullet,
  including its continuation lines.
- If the ADR edit breaks the sentence, re-read lines 13-17 of the ADR and
  rewrite the single sentence only.

Success criteria: the command above returns exit code 1 (no matches) when run
with the pattern `src/ir/exec-ir\.hpp|src/interp/ir-vm|src/ir/hir-to-ir`.

---

### Step 2 - Remove the residual merge file - DONE

Goal: `src/vm/typed-ir.hpp.orig` no longer exists on disk.

Sub-steps:

1. Confirm the file is the only `.orig` under `src/` and `tests/`.

Command:

    cd /home/diogo/Zith && find src tests -name '*.orig' -o -name '*.rej'

Expected output: exactly one line, `src/vm/typed-ir.hpp.orig`.

2. Delete it.

Command:

    rm -f /home/diogo/Zith/src/vm/typed-ir.hpp.orig

3. Confirm it is gone.

Command:

    cd /home/diogo/Zith && find src tests -name '*.orig' -o -name '*.rej'

Expected output: no lines, exit code 0.

Failure checks:

- If the delete command reports a read-only filesystem error, rerun it with the
  sandbox escalation for `rm -f /home/diogo/Zith/src/vm/typed-ir.hpp.orig`.
- If `find` still prints the file, the delete failed; report the exact error.

Success criteria: `find` prints nothing and `git status --short` is unchanged
for tracked files (the file is gitignored, so it must not appear in git output).

---

### Step 3 - Give the two flagged members default initializers - DONE

Goal: `HirFunction::return_type` and `Options::subcommandArg` have
default-member-initializers, so a future call site cannot read an indeterminate
value.

Sub-steps:

1. Open `/home/diogo/Zith/src/memory/string-interner.hpp`.
2. Immediately after `using InternedId = uint32_t;` (line 13), add:
   `inline constexpr InternedId kInvalidInternedId = ~InternedId{0};`
3. Open `/home/diogo/Zith/src/hir/hir-module.hpp`.
4. Change line 22 from `memory::InternedId name;` to
   `memory::InternedId name = memory::kInvalidInternedId;`.
5. Change line 28 from `HirTypeId return_type;` to
   `HirTypeId return_type = types::kInvalidType;`.
6. Open `/home/diogo/Zith/src/cli/options.hpp`.
7. Change line 276 from `memory::InternedId subcommandArg;` to
   `memory::InternedId subcommandArg = memory::kInvalidInternedId;`.

Command:

    cmake --build /home/diogo/Zith/build -j4

Expected output: the build completes with exit code 0 and no new warnings. With
Clang and `ZITH_ENABLE_STRICT_WARNINGS` on, `-Werror` must stay clean.

Failure checks:

- If the compiler reports `no member named 'kInvalidInternedId' in namespace
  'memory'`, step 2 was skipped or placed in the wrong header. Confirm the line
  exists in `src/memory/string-interner.hpp` and that the file is included.
- If the build reports `unused variable` or a `-Werror` failure, stop and report
  the exact diagnostic verbatim; do not add `(void)` casts.

Then run the focused tests:

Command:

    ctest --test-dir /home/diogo/Zith/build -R 'cli-commands|hir|frontend' --output-on-failure

Expected output: `100% tests passed, 0 tests failed out of <N>`.

Failure checks:

- If a test fails, run it directly (for example
  `/home/diogo/Zith/build/test-cli-commands`) and report the failing assertion
  verbatim. The default values must not change any observed behaviour.

Success criteria: the full build and the focused tests both exit 0.

---

### Step 4 - Track the VM/interp coverage gap - DONE

Goal: the "no LLVM builds the VM path that no test exercises" gap is recorded
in the cleaning queue, so it is not lost.

Sub-steps:

1. Open `/home/diogo/Zith/memory/audit-cleaning.md`.
2. In the "Cleaning Queue" list, append this bullet:
   "`src/cli/cmd/run.cpp` fixes `useIrVm` to `false` with LLVM and `true`
   without, so the `useIrVm` block is unreachable in a normal build and no test
   covers it. Decide whether the no-LLVM path gets a dedicated CI job or is
   removed, then make `test-vm-v2` skip instead of fail when `src/vm/` is
   excluded."

Command:

    cd /home/diogo/Zith && rg -n "useIrVm" memory/audit-cleaning.md

Expected output: one matching line containing `src/cli/cmd/run.cpp`.

Failure checks:

- If `rg` prints nothing, the bullet was not added; re-open the file and append
  it under the "Cleaning Queue" heading.

Success criteria: the bullet is present and mentions `useIrVm` and
`test-vm-v2`.

---

### Step 5 - Final acceptance check - DONE

Goal: every finding in debt sections A-D is either fixed or explicitly tracked,
and the tree builds and tests clean.

Sub-steps:

1. Confirm no stale `src/ir` path remains in the fixed files.

Command:

    cd /home/diogo/Zith && rg -n "src/ir/(exec-ir|hir-to-ir)|src/interp/ir-vm" docs/adr/0024-wasm-vm-v2-abi.md memory/audit-cleaning.md memory/agent7-formatter-build.md

Expected output: no lines, exit code 1.

2. Confirm no `.orig`/`.rej` remains.

Command:

    cd /home/diogo/Zith && find src tests -name '*.orig' -o -name '*.rej'

Expected output: no lines.

3. Rebuild and run the full suite.

Command:

    cmake --build /home/diogo/Zith/build -j4 && ctest --test-dir /home/diogo/Zith/build --output-on-failure

Expected output: build exit code 0; `100% tests passed, 0 tests failed`.

4. Reindex the docs corpus so the memory search does not return the old text.

Command:

    cd /home/diogo/Zith && python3 scripts/rag.py index

Expected output: an index summary with a non-zero docs chunk count and exit
code 0.

5. Review the diff.

Command:

    cd /home/diogo/Zith && git diff --stat && git status --short

Expected output: the diff touches only the files listed in this plan's header,
plus the user's pre-existing uncommitted NRA files, which must be left
untouched.

Failure checks:

- If the full suite fails on a test unrelated to these edits, run that test
  alone, capture the output, and report it verbatim. Do not mark the plan done.
- If `git status --short` shows edits to `docs/nra-spec.md`,
  `docs/adr/0033-nra-reference-model-and-bind.md`, `docs/adr/0034-*`,
  `docs/adr/0035-*`, or `memory/nra-design.md` that you did not make, stop and
  report; those are the user's in-flight changes.

Success criteria: steps 1-3 produce the expected clean output and the diff is
limited to this plan's scope.
