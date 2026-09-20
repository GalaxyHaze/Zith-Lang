# Plan: Lazy Skills for C++

Objective: create and document a standalone public repository named
`lazy-skills` that distributes a focused set of agent skills for C++ projects,
an independent installer, a `lazy init` cataloger, a `lazy scaffold` generator,
and local validation scripts, while only recommending (not forking) the
relevant Matt Pocock skills.

Preconditions:

- The current working directory for implementation is `/home/diogo/Zith`.
- The new repository must live outside Zith, at `/home/diogo/lazy-skills`.
- The repository will be public on GitHub.
- Only Python standard library scripts are allowed.
- The implementation must work without a ByteAsk plugin and without GitHub
  Actions.
- The existing skills from Matt are not to be copied into the new repository.
- The worktree of the new repository does not need pre-existing commit
  history.

Files that will be created under `/home/diogo/lazy-skills`:

- `README.md`
- `LICENSE`
- `CREDITS.md`
- `docs/CONTEXT.md`
- `docs/upstream.md`
- `docs/usage-cpp.md`
- `lazy.toml`
- `scripts/lazy-install.py`
- `scripts/lazy-init.py`
- `scripts/lazy-scaffold.py`
- `scripts/lazy-harness.py`
- `scripts/check.sh`
- `skills/lazy-init/SKILL.md`
- `skills/lazy-harness/SKILL.md`
- `skills/sequentia/SKILL.md`
- `skills/agent-worktrees/SKILL.md`
- `skills/coordinate-agents/SKILL.md`
- `skills/memory/SKILL.md`
- `skills/plan-slice/SKILL.md`
- `skills/review-gate/SKILL.md`
- `seq/v2-slice/module.sequenta`
- `seq/v2-slice/context.sequenta`

Files that must NOT be edited:

- `/home/diogo/Zith/AGENTS.md`
- `/home/diogo/Zith/CONTEXT.md`
- `/home/diogo/Zith/src/**`
- `/home/diogo/Zith/tests/**`
- `/home/diogo/.byteask/skills/**` except for the copy command in the installer
  test, which must use a temporary directory instead.

## Step 1 - Create the repository skeleton

Goal: `/home/diogo/lazy-skills` exists with the public repo directories and
top-level documentation placeholders that are ready for content.

Sub-steps:

1. Create `/home/diogo/lazy-skills` if it does not exist.
2. Create the directories `scripts`, `skills`, `docs`, and `seq`.
3. Run `git init` inside `/home/diogo/lazy-skills`.
4. Write a minimal `.gitignore` that ignores Python bytecode and temporary
   files.
5. Write `README.md` with the one-sentence purpose of the repo.
6. Write `LICENSE` containing the MIT license text.
7. Write `CREDITS.md` acknowledging Matt Pocock as the upstream author of
   recommended skills and listing the skills in this repository.

Commands:

    mkdir -p /home/diogo/lazy-skills/scripts
    mkdir -p /home/diogo/lazy-skills/skills
    mkdir -p /home/diogo/lazy-skills/docs
    mkdir -p /home/diogo/lazy-skills/seq
    cd /home/diogo/lazy-skills && git init

Expected output:

- `git init` prints `Initialized empty Git repository in /home/diogo/lazy-skills/.git/`.
- The tool `ls /home/diogo/lazy-skills` lists `scripts`, `skills`, `docs`,
  `seq`, `.gitignore`, `README.md`, `LICENSE`, and `CREDITS.md`.

Failure checks:

- If `/home/diogo/lazy-skills` already has files, inspect them and merge, do
  not delete them. If the directory contains private project data, stop and
  report the path contents verbatim.
- If `git init` reports `already exists`, continue and verify the repository
  with `git -C /home/diogo/lazy-skills status --short`.

Success criteria: `git -C /home/diogo/lazy-skills status --short` exits 0 and
`README.md`, `LICENSE`, and `CREDITS.md` exist.

## Step 2 - Define the manifest and installer contract

Goal: `lazy.toml` defines the canonical skill catalog and
`scripts/lazy-install.py` independently installs selected skills to a
user-configurable directory.

Sub-steps:

1. Write `/home/diogo/lazy-skills/lazy.toml` with a `version`, `default_dest`,
   and `skills` table.
2. Each skill in the manifest must have a `name`, `source_dir`, `description`,
   and whether it is `lazy_owned` or `recommended`.
3. Include these lazy-owned skills in the manifest:
   `lazy-init`, `lazy-harness`, `sequentia`, `agent-worktrees`,
   `coordinate-agents`, `memory`, `plan-slice`, and `review-gate`.
4. Include `handoff` and the Matt flow skills as `recommended` entries that
   point to `docs/upstream.md` instead of local source directories.
5. Write `/home/diogo/lazy-skills/scripts/lazy-install.py` using only
   `argparse`, `json`, `os`, `pathlib`, `shutil`, and `sys`.
6. The script reads `lazy.toml`, presents the list of installable skills, and
   accepts `--skill NAME` (repeatable), `--dest PATH`, `--dry-run`, `--check`,
   and `--uninstall`.
7. Default `--dest` is `~/.byteask/skills`; override it with `LAZY_SKILLS_DIR`
   or `--dest`.
8. The install action must copy skill directories, never symlink them.
9. The `--check` action must verify that every selected skill directory exists
   and that `SKILL.md` is present.
10. The `--uninstall` action must remove only the exact skill directories it
    installed, never the whole destination directory.

Commands:

    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-install.py --help
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-install.py --skill lazy-init --dest /tmp/lazy-skills-test --dry-run

Expected output:

- `--help` lists `--skill`, `--dest`, `--dry-run`, `--check`, and
  `--uninstall`.
- The `--dry-run` command prints the path of `/tmp/lazy-skills-test/lazy-init`
  and does not create it.

Failure checks:

- If the script writes anywhere under `/tmp` without `--dry-run`, stop and
  report the unplanned writes.
- If a manifest skill is missing its `SKILL.md`, report the missing file and
  stop.
- Python syntax errors must be resolved before proceeding.

Success criteria:

    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-install.py --skill lazy-init --dest /tmp/lazy-skills-test --check

prints `ok` and exits 0.

## Step 3 - Implement `lazy-init` cataloger

Goal: `lazy-init` proposes Sequenta modules and cards from an existing C++
repository without mutating user-controlled documents without dry-run and
confirmation.

Sub-steps:

1. Write `/home/diogo/lazy-skills/scripts/lazy-init.py` using only Python
   standard library modules.
2. Accept `--repo PATH` (default current directory), `--out PATH` (default
   `seq`), `--dry-run`, `--json`, `--confirm`, and `--test-cmd COMMAND`.
3. Read `lazy.toml` from the repository root when present.
4. Treat the following as input roots when they exist:
   `src`, `tests`, `stdlib`, `docs/plans`, `memory`, `AGENTS.md`,
   `CONTEXT.md`, and `docs/adr`.
5. For each existing root, propose one top-level module named after the root.
6. For `src`, use directory names as card symbols; for `tests`, use the name
   of each test executable; for `docs/plans`, use each plan filename without
   `.md`.
7. Write generated files under `seq/` as `seq/<module>/module.sequenta` and
   `seq/<module>/context.sequenta`.
8. Every generated module must contain at least a `module.sequenta` declaration
   and a matching `context.sequenta` section.
9. Do not edit `AGENTS.md`, `CONTEXT.md`, `docs/adr`, or any source file.
10. When `--confirm` is passed, print a compact summary of planned files and
    ask for an explicit yes or no before writing.

Commands:

    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-init.py --repo /home/diogo/Zith --dry-run --json
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-init.py --repo /home/diogo/Zith --out /tmp/lazy-init-out --confirm

Expected output:

- The `--json` output is valid JSON with a `modules` array.
- The confirm mode prints each module path and waits for `yes`.
- After confirmation, `/tmp/lazy-init-out/<module>/module.sequenta` and
  `context.sequenta` exist.

Failure checks:

- If the script attempts to write to `/home/diogo/Zith` itself, stop. It must
  use the `--out` temporary directory.
- If JSON is invalid, stop and report the message.
- If a proposed symbol collides with an existing Sequenta symbol, keep both
  only when the generated module names differ.

Success criteria: the temporary output contains at least one `module.sequenta`
whose `sequenta compile` command succeeds when run from the temporary output
root.

## Step 4 - Implement `lazy-scaffold` generator

Goal: `lazy-scaffold` creates a minimal C++ repository layout that matches the
expected ontology used by `lazy-init`.

Sub-steps:

1. Write `/home/diogo/lazy-skills/scripts/lazy-scaffold.py` using only Python
   standard library modules.
2. Accept `--name NAME`, `--dest PATH` (default current directory), and
   `--dry-run`.
3. Create `src/`, `tests/`, `docs/plans/`, `docs/adr/`, and `memory/`.
4. Create a minimal `CMakeLists.txt` that enables CTest.
5. Create `AGENTS.md` only when `--create-agents` is passed.
6. Create `CONTEXT.md` with a one-line project description.
7. Create `memory/README.md` and `docs/plans/README.md` as placeholders.
8. Print the final tree with `find` after successful generation.

Commands:

    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-scaffold.py --name demo --dest /tmp/lazy-skills-scaffold --dry-run
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-scaffold.py --name demo --dest /tmp/lazy-skills-scaffold --create-agents

Expected output:

- The dry run prints the files it would create without creating them.
- The real run creates the directories listed above.

Failure checks:

- If the destination already contains files, do not overwrite them; print an
  error and exit nonzero.
- If a directory creation fails, report the `OSError` message and stop.

Success criteria: `find /tmp/lazy-skills-scaffold -maxdepth 2 -type f` lists
`CMakeLists.txt`, `CONTEXT.md`, `AGENTS.md`, `memory/README.md`, and
`docs/plans/README.md`.

## Step 5 - Add the fine router skill

Goal: `lazy-harness` is a context-pointer router that selects the correct
skill for the current phase without owning state.

Sub-steps:

1. Create `/home/diogo/lazy-skills/skills/lazy-harness/SKILL.md`.
2. Frontmatter must set `name: lazy-harness` and a description that triggers
   when a user asks to start or continue a structured C++ task.
3. The body must route by phase:
   - catalog or scaffold -> `lazy-init` or `lazy-scaffold`.
   - slice plan -> `plan-slice`.
   - execute card -> `sequentia`.
   - parallel worktree -> `agent-worktrees` plus `coordinate-agents`.
   - project memory -> `memory`.
   - final gating -> `review-gate`.
4. The router must state that it owns no state and that the state remains in
   `.sequenta/`.
5. It must not describe implementation details of other skills.

Command:

    /usr/bin/env python3 -c "import yaml, pathlib; data=yaml.safe_load((pathlib.Path('/home/diogo/lazy-skills/skills/lazy-harness/SKILL.md').read_text())); print(data['name'])"

Expected output: `lazy-harness`.

Failure checks:

- If the skill uses absolute paths, replace them with relative references.
- If it duplicates Sequenta details, remove the duplicate.

Success criteria: the file exists and contains the six routing lines listed
above.

## Step 6 - Seed the Sequenta card files

Goal: `seq/v2-slice` contains a valid Sequenta module and context sheet that
documents the harness work as bounded cards.

Sub-steps:

1. Write `/home/diogo/lazy-skills/seq/v2-slice/module.sequenta` with numbered
   tasks for `lazyInit`, `lazyScaffold`, `lazyHarness`, `memorySkill`,
   `planSlice`, and `reviewGate`.
2. Each task must define a signature and a `use`/`depends_on` block where
   appropriate.
3. Write `/home/diogo/lazy-skills/seq/v2-slice/context.sequenta` with
   `desc`, `impl`, and a deterministic `tests` table for each task.
4. Run the local Sequenta script against the new directory.

Command:

    python3 /home/diogo/.byteask/skills/sequentia/scripts/sequenta compile /home/diogo/lazy-skills/seq

Expected output: `ok: <N> task(s), <M> module(s)`.

Failure checks:

- If a symbol is undeclared, add the declaration in the module file before
  recompiling.
- If a dependency cycle is reported, remove the offending dependency.

Success criteria: the compile command exits 0 and reports at least one task.

## Step 7 - Add the remaining owned skills

Goal: `memory`, `plan-slice`, `review-gate`, `lazy-init`, `sequentia`,
`agent-worktrees`, and `coordinate-agents` each have a focused, self-contained
`SKILL.md`.

Sub-steps:

1. Create `skills/memory/SKILL.md` to describe durable `memory/*.md` notes and
   privacy rules.
2. Create `skills/plan-slice/SKILL.md` to describe converting a feature into
   Sequenta slices and cards.
3. Create `skills/review-gate/SKILL.md` to describe a C++ merge checklist:
   `cmake --build`, `ctest`, docs, ADR, and memory consistency.
4. Create `skills/lazy-init/SKILL.md` to explain when to call the `lazy-init`
   script and when to use dry-run.
5. Copy `SKILL.md`, `scripts`, `references`, and `agents` from the current
   `sequentia` skill directory into the new repo location.
6. Copy the `agent-worktrees` skill directory from the current installation
   into the new repo.
7. Copy the `coordinate-agents` skill directory from the current installation
   into the new repo.
8. Replace any absolute path under `/home/diogo/.byteask/skills` with a
   relative path or a documented `LAZY_SKILLS_DIR` variable.

Commands:

    cp -R /home/diogo/.byteask/skills/sequentia /home/diogo/lazy-skills/skills/
    cp -R /home/diogo/.byteask/skills/agent-worktrees /home/diogo/lazy-skills/skills/
    cp -R /home/diogo/.byteask/skills/coordinate-agents /home/diogo/lazy-skills/skills/

Expected output:

- Every directory under `skills/` contains a `SKILL.md`.
- Absolute paths do not appear in the copied skill bodies.

Failure checks:

- If a copied skill still contains `/home/diogo`, stop and replace the
  absolute path with a relative path.
- Do not copy skills from Matt such as `handoff`, `tdd`, `research`,
  `code-review`, `grilling`, or `domain-modeling`.

Success criteria:

    find /home/diogo/lazy-skills/skills -mindepth 1 -maxdepth 2 -name SKILL.md

prints the eight expected skill entries.

## Step 8 - Validate scripts and generated Sequenta

Goal: every script passes local validation and every Sequenta file compiles.

Sub-steps:

1. Run `python3 -m py_compile` on each Python script.
2. Run the Sequenta compile on `seq/`.
3. Run the installer `--check` against a temporary directory.
4. Run the init dry-run against a temporary empty repository.
5. Run a scaffold smoke test in a temporary directory.

Commands:

    python3 -m py_compile /home/diogo/lazy-skills/scripts/*.py
    python3 /home/diogo/.byteask/skills/sequentia/scripts/sequenta compile /home/diogo/lazy-skills/seq
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-install.py --check --dest /tmp/lazy-skills-check
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-init.py --repo /tmp/lazy-skills-empty --dry-run --json
    /usr/bin/env python3 /home/diogo/lazy-skills/scripts/lazy-scaffold.py --name smoke --dest /tmp/lazy-skills-smoke

Expected output:

- `py_compile` exits 0.
- Sequenta compile prints `ok: ...`.
- Installer check prints `ok`.
- Init dry-run emits JSON and creates no files outside `/tmp`.
- Scaffold smoke creates `/tmp/lazy-skills-smoke/CMakeLists.txt`.

Failure checks:

- If a Python module has a syntax error, fix the exact file and retry.
- If Sequenta reports a dependency error, follow Step 6 recovery.
- If install check cannot find a skill, verify the manifest path and the skill
  directory.

Success criteria: all five commands exit 0.

## Step 9 - Write documentation and credits

Goal: the repository is self-describing for a new user and clearly separates
owned skills from recommended upstream skills.

Sub-steps:

1. Write `README.md` with quickstart, installation command, and repository
   layout.
2. Write `docs/CONTEXT.md` with the project ontology and the expected C++
   layout.
3. Write `docs/usage-cpp.md` with a worked example using `lazy-init`,
   `sequentia`, worktrees, and `review-gate`.
4. Write `docs/upstream.md` listing Matt Pocock's original skills, their
   purpose, and the upstream source URL.
5. Update `CREDITS.md` to cite every upstream skill mentioned.
6. Update `LICENSE` if needed to include only the project license.

Commands:

    cat /home/diogo/lazy-skills/README.md
    cat /home/diogo/lazy-skills/docs/upstream.md
    cat /home/diogo/lazy-skills/CREDITS.md

Expected output: each file is present and has no unresolved placeholder.

Failure checks:

- If docs mention `/home/diogo`, treat it as a portability bug and replace it
  with `$HOME` or `LAZY_SKILLS_DIR`.
- If the quickstart command does not match `lazy-install.py`, fix it.

Success criteria: `rg -n "TODO|PLACEHOLDER|/home/diogo" /home/diogo/lazy-skills/docs /home/diogo/lazy-skills/README.md` returns exit code 1 (no matches).

## Step 10 - Final repository acceptance

Goal: `/home/diogo/lazy-skills` is complete, documented, and locally validated.

Sub-steps:

1. Run the full local validation again.
2. Confirm no files from Zith were modified by the scripts.
3. Confirm no upstream skill directories are included in `skills/`.
4. Confirm the Sequenta state files are present under `seq/.sequenta` if a
   checkpoint was recorded, otherwise omit state.
5. Report the repository status and any pending implementation decisions.

Commands:

    bash /home/diogo/lazy-skills/scripts/check.sh
    find /home/diogo/lazy-skills -maxdepth 3 -type f | sort

Expected output:

- `check.sh` exits 0.
- The file list contains the expected scripts, skills, docs, and `seq` files.
- `git -C /home/diogo/Zith status --short` remains unchanged from the state at
  the time this plan was written.

Failure checks:

- If `git -C /home/diogo/Zith status --short` shows new changes, stop and
  report the diff before continuing.
- If any script depends on `~/.byteask/skills`, change it to use the manifest
  or an explicit repository-relative path.

Success criteria: all acceptance checks in Steps 1 through 9 pass, and the
repository can generate the expected `skills/`, `seq/`, `scripts/`, and
`docs/` directories from a fresh clone.
