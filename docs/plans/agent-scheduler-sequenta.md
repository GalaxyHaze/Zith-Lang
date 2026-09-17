# Agent Scheduler Stability Plan and Sequenta Integration

Objective: stabilise the current agent-worktree scheduler by giving each agent a
concrete polling script and by teaching the master to merge, test, advance the
queue, collect debt reports, and stop cleanly when the manifest is exhausted. A
later wave integrates the Sequenta DSL as the canonical task/state layer without
replacing `agent-worktrees`.

Status: design closed, implementation in progress.

## Current State

`scripts/agent-scheduler/scheduler.py` is the master polling coordinator. It
watches `.awt/requests/<agent>.md`, reviews and optionally merges agent
branches through the `awt` scripts, and writes the next `TASK.md` or an `end`
marker into `.awt/<agent>/TASK.md`. It keeps state in
`.awt/scheduler-status.json` and logs events under `.awt/events/<agent>.log`.

`agent-worktrees` provides the git isolation, merge requests, review, merge and
rebase boundary. `work-agent` tells an agent to return to a listening loop after
a merge request, but does not define how that loop stays alive. In practice the
agent tries briefly and then stops, and pending requests stay unprocessed until
the human master runs the scheduler.

`multitask-plan` currently creates worktrees and prints launch prompts; it does
not launch or supervise sessions. No launcher exists for the agent loop.

Sequenta is implemented as a separate skill under
`/home/diogo/.byteask/skills/sequentia`. It is a text-first task DSL with
`Module::Symbol` tasks, module/context sheets, `.sequenta/state.json` and a CLI.
The v1 CLI intentionally does not call `awt`. `sequenta compile` and
`sequenta next` were verified against the examples; `orient` was also verified
after fixing a read-only bug that made it abort on workspaces where `.sequenta/`
could not be written.

## Contract

### Roles

- Master/scheduler owns merging, testing, queue advancement and debt collection.
- Agent owns implementing the current `TASK.md`, checking in, requesting merge,
  and running a polling script after the request until the next task or `end`.
- The agent does not delete requests or merge into master.
- Auto-merge is allowed after `awt review` passes review gates; no human
  confirmation is required for ordinary requests.
- MCP/event-bus redesign is out of scope for this wave.

### Files

- `.awt/requests/<agent>.md`: merge request signal, created by `awt
  request-merge`, consumed and removed by `awt merge`.
- `.awt/<agent>/TASK.md`: current task contract written by the master.
- `.awt/agent-state/<agent>.json`: agent lifecycle state written only by the
  scheduler. States are `running`, `waiting`, `blocked`, `ended`.
- `.awt/debts/<agent>.md`: new debt report per agent, consumed by the master.
- `.awt/scheduler-status.json`: compatibility mirror; queue state and seen
  merge requests are authoritative in `.awt/agent-state/`.
- `scripts/agent-scheduler/agent-wait.sh`: polling loop for one agent.

### Agent Wait Contract

The agent session runs `scripts/agent-scheduler/agent-wait.sh` after it
requests merge. The script:

1. Reads the agent worktree root from the first argument or from the current
   worktree.
2. Polls `<worktree>/TASK.md` at a default interval of 5 seconds.
3. Watches the `task-sequence:` line in `TASK.md`; it exits when that value
   changes from the value present when the script started. This avoids a
   restart trap where the master writes an identical task body.
4. When `task-sequence:` changes, prints the new content to stdout and exits 0.
5. When `TASK.md` contains `# Status` and `end`, prints the file and exits 0;
   the caller treats the output as the final terminal marker.
6. Never removes `.awt/requests/<agent>.md`.
7. Must not write into `.awt/agent-state/`; the scheduler is the single writer
   for that directory. The script may write no heartbeat at all.

The poller returns exit 0 for both a new task and `end`; the caller decides by
inspecting the printed content. This keeps a simple contract: a non-zero exit
is an error, and output contains the current authoritative task body. The
scheduler writes `task-sequence: N` to `TASK.md` for every dispatch, including
the initial setup and every advance.

### Scheduler Contract

Production mode is:

```bash
python3 scripts/agent-scheduler/scheduler.py --watch --merge --interval 5
```

The scheduler, at each poll:

1. Loads the authoritative per-agent state from `.awt/agent-state/` and keeps
   `.awt/scheduler-status.json` as a compatibility mirror.
2. Parses `.awt/manifest.md` plans.
3. For each plan with a merge request, records the event.
4. Runs `awt review <repo> <agent>` before giving the merge signal.
5. Runs the agent worktree tests before granting the merge.
6. Runs `awt merge <repo> <agent>` when review and agent-tests pass review gates.
7. Runs the consolidated build and test suite after a successful merge.
8. If integration tests pass, advances the agent to the next task or writes `end`.
9. If integration tests fail, marks only that agent as `blocked` with evidence
   and keeps other agents moving.
10. Writes `end` when the manifest queue for an agent is exhausted and all its
    assigned tasks are done.
11. Marks an agent `blocked` with `reason: manifest_exhausted_without_done`
    when the manifest ends but its done list is incomplete.
12. Exits 0 when every agent is ended or blocked and no request can unblock
    them.
13. Uses `--max-wait SECONDS` to stop waiting when no agent event arrives for
    that long; default is 600 seconds. A timeout is not success: it exits
    nonzero and leaves pending requests and agent state untouched for manual
    audit.

Test failure on consolidated master is not a full-lot stop. Other agents
continue; the failing agent stays blocked until a human resolves integration or
re-runs with a cleaner manifest. The blocked state stores at least:
`status: blocked`, `failed_head`, `merged_head`, `failed_stage: build|ctest`,
`acted_on: false`, and `reason`.

Agent worktree tests run before merge. The scheduler uses the task file as the
test command source or falls back to the repo's focused test command for the
agent's touched area. The consolidated master build and CTest run after merge
as integration verification.

### Debt Report Contract

Agents write `.awt/debts/<agent>.md` when they find a new debt. Format:

```markdown
# <Agent> Debt Report

- Debt: <optional id or short name>
 - Evidence: <file/path or command output that proves the debt>
 - Scope: <what is out of scope or what would be needed>
```

The scheduler collects `.awt/debts/*.md` at the end of the wave and copies them
into `docs/implementation-debt.md` only after human review. It generates
`.awt/debts-pending.md` with the report paths and a short summary; it does not
append new debts automatically to source-controlled docs.

### Shutdown And New Goals

When all agents are ended or blocked, the scheduler exits with status 0. An
agent is `ended` only when every task in its manifest section is in the done
list. If the manifest ends with unfinished tasks, the scheduler marks the
agent `blocked` with `reason: manifest_exhausted_without_done`. The user starts
a new wave by editing `.awt/manifest.md`, running reset/setup, and starting a
new watch:

```bash
python3 scripts/agent-scheduler/scheduler.py --require-clean --reset-queue
python3 scripts/agent-scheduler/scheduler.py --require-clean --setup
python3 scripts/agent-scheduler/scheduler.py --require-clean --watch --merge
```

## Pending Review: agent2 and agent4

Current `.awt/requests/agent2.md` and `.awt/requests/agent4.md` are stale
candidates from 2026-09-11. Do not merge them before the stability work is
verified. When auditing:

1. Run `awt review /home/diogo/Zith agent2` and `awt review
   /home/diogo/Zith agent4`.
2. Compare request `head` with the branch head:
   `git -C /home/diogo/Zith rev-parse awt/agent2`, and the same for agent4.
3. Confirm `git log --oneline --decorate origin/main..awt/agent2` (and agent4)
   only contains agent-owned commits for the manifest task.
4. Check whether any commit is already in `origin/main`; if so, report duplicate
   merge risk and do not merge.
5. Build and run the focused tests for the touched subsystems before accepting.
6. Merge only after the new scheduler flow is stable and the review gates are
   green.

## Implementation Plan

The plan below is for the executor who will implement this document after the
user accepts it.

Preconditions:

- Working directory is `/home/diogo/Zith`.
- Git worktree master is currently dirty with unrelated user changes.
- Do not touch user-modified files under `src/` except where the scheduler
  work explicitly requires it.
- Do not run `git reset --hard`, `git checkout --`, or `git clean`.

Files modified in this wave:

- `scripts/agent-scheduler/agent-wait.sh`
- `scripts/agent-scheduler/scheduler.py`
- `scripts/agent-scheduler/README.md`
- `/home/diogo/.byteask/skills/work-agent/SKILL.md`
- `/home/diogo/.byteask/skills/coordinate-agents/SKILL.md`
- `/home/diogo/.byteask/skills/agent-worktrees/SKILL.md` if the polling contract
  moves into the skill
- `memory/agent-scheduler.md` new memory file
- `docs/plans/agent-scheduler-sequenta.md` this file

### Step 1 - Add `agent-wait.sh`

Goal: `scripts/agent-scheduler/agent-wait.sh` blocks until `TASK.md` changes and
prints the new content.

Sub-steps:

1. Create `/home/diogo/Zith/scripts/agent-scheduler/agent-wait.sh` with shebang
   `#!/usr/bin/env bash`, `set -euo pipefail`.
2. Accept `REPO` as `$1`, `AGENT` as `$2`, optional `INTERVAL` as `$3`.
3. Resolve `TASK="$REPO/.awt/$AGENT/TASK.md"`, default `INTERVAL=5`, require
   integer >= 1.
4. Snapshot the `task-sequence:` value and current file content before the
   loop.
5. Loop: sleep `INTERVAL`, re-read; if the `task-sequence:` value differs from
   the snapshot, print the whole current file to stdout and exit 0.
6. If `TASK.md` contains `# Status` and `end`, print the file and exit 0.
7. On missing `TASK.md` at start, exit 2 with an explanatory message.
8. Never touch `.awt/requests`, `.awt/agent-state`, or the git index.

Command to create the file with apply_patch or a heredoc after patch review:

```bash
chmod +x /home/diogo/Zith/scripts/agent-scheduler/agent-wait.sh
```

Expected output: the script exists, is executable, and accepts `--help`.

Failure checks:

- `TASK.md` missing: the script must exit 2 and say which worktree is wrong.
- `INTERVAL` not integer: exit 2 and print the valid range.
- The first changed content is `end`: exit 0 and print the terminal marker.
- The file lacks `task-sequence:`: the script must still treat `end` as
  terminal and otherwise exit 2 with a clear compatibility error, because the
  scheduler writes this marker on every dispatch.

Success criteria: in a scratch copy of the repo with a controlled `TASK.md`,
the script prints new content when `task-sequence:` changes and exits 0.

### Step 2 - Add agent state files support

Goal: scheduler can read and write `.awt/agent-state/<agent>.json`.

Sub-steps:

1. Add constants `AGENT_STATE` and `DEBTS` to `scheduler.py`.
2. Add `load_agent_state()` and `save_agent_state()`.
3. Use states `running`, `waiting`, `blocked`, `ended` only.
4. Keep existing `scheduler-status.json` as a compatibility mirror only; agent
   state files are authoritative.
5. Make the scheduler the single writer for `.awt/agent-state/<agent>.json`.
6. `agent-wait.sh` must not write into `.awt/agent-state/`.

Expected output: after initialization, `.awt/agent-state/` contains one JSON
file per manifest agent.

### Step 3 - Teach scheduler to run tests after merge

Goal: after a successful merge, scheduler runs build and CTest before advancing.

Sub-steps:

1. Add function `run_tests()` that runs:
   `cmake --build /home/diogo/Zith/build -j4` and
   `ctest --test-dir /home/diogo/Zith/build --output-on-failure`.
2. Call it only after a merge that was not already merged/ledgered.
3. Run agent worktree tests before merge, using the task file command or a
   conservative fallback, and refuse merge if those tests fail.
4. If post-merge `run_tests` fails, write `blocked` into
   `.awt/agent-state/<agent>.json` with `failed_stage`, `failed_head`,
   `merged_head`, `acted_on: false` and `reason`, append an event, and keep the
   metadata visible for the human.
5. If both test stages pass, advance the agent normally.

Expected output: scheduler log shows `agent tests ok`, `integration tests ok`
after merge, or `integration tests failed; blocked <agent>`.

Failure checks:

- Build can write into `build/`; if the command fails because of dirty master,
  report the first compiler error and stop the scheduler rather than advancing.
- If CTest fails, mark blocked and do not retry in the same watch loop without
  human intervention.

### Step 4 - Collect debt reports at shutdown

Goal: scheduler lists `.awt/debts/*.md` for human review when the wave ends.

Sub-steps:

1. At shutdown, print a summary of debt report paths if any exist.
2. Do not write debts directly into `docs/implementation-debt.md`.
3. Generate `.awt/debts-pending.md` with the report paths and short summaries.
4. Generate a board/status summary under `.awt/board.md` containing agent,
   current task, status, and debt report link.

Expected output: `.awt/board.md` exists when the wave ends and reports debts.

### Step 5 - Update skills and docs

Goal: skills no longer say "return to listening loop" without a mechanism.

Sub-steps:

1. In `work-agent/SKILL.md`, replace the waiting paragraph with an explicit call
   to `agent-wait.sh` and the rule that after exit 0 the agent inspects the
   printed `TASK.md`.
2. In `coordinate-agents/SKILL.md`, document `--merge`, post-merge tests,
   blocked state, debt collection, and clean shutdown.
3. Update `scripts/agent-scheduler/README.md` with the new commands and
   `.awt/agent-state`/`.awt/debts`.
4. Create `memory/agent-scheduler.md` with the contract and gotchas.

Expected output: all references to the old "listening loop" without a script
are gone.

### Step 6 - Verify the scheduler loop

Goal: a controlled run with two synthetic agents proves merge, test, advance
and shutdown.

Sub-steps:

1. Create a scratch manifest with two trivial agents in a scratch repo.
2. Create one fake request and run `scheduler.py --watch --merge`.
3. Confirm review/merge passes or is reported, tests fail if scratch build is
   absent, and one agent is marked blocked while the other advances.
4. Remove the scratch repo or keep it under `/tmp`.

Expected output: scheduler exits or blocks with explicit per-agent status;
no request is silently left pending.

## Sequenta Integration (Future Wave)

Sequenta is implemented but not yet wired into this scheduler stability wave.
The boundary remains:

- Sequenta owns the canonical task graph, per-task state and orientation.
- `agent-worktrees`/scheduler owns git isolation, merge review, tests and
  queue advancement.
- The thin bridge should translate the manifest into Sequenta modules when the
  user enables it, then read `.sequenta/state.json` instead of
  `scheduler-status.json` for readiness.

The functional rationale is complementary: Sequenta alone gives a cheap,
resumable, focused worker, but it depends on nobody mutating state outside the
CLI and is vulnerable under shared concurrent execution. Worktrees remove that
shared-state pressure by giving each agent an isolated linear execution path.
So Sequenta is only useful in practice as an extension of the worktree flow,
not a replacement for it.

Planned future commands when the Sequenta CLI is installed:

```bash
sequenta compile /home/diogo/Zith
sequenta orient /home/diogo/Zith --module auth::login --out /home/diogo/Zith/.awt/agent1/orientation.md
awt checkin /home/diogo/Zith "checkpoint auth::login"
awt request-merge /home/diogo/Zith "auth::login done"
```

Do not couple `scheduler.py` to the `sequenta` CLI until the bridge is
implemented and the CLI is available from the repo or from `PATH`.

## Acceptance Checklist

- `agent-wait.sh` blocks and returns new `TASK.md` content on change.
- Agent skills name a concrete waiting command.
- Scheduler runs consolidated tests after merge before advancing.
- Failed merged tests mark only the affected agent `blocked`.
- Debt reports are collected for human review, not auto-committed.
- `agent2`/`agent4` are audited before merge under the pending-review process.
- The scheduler exits cleanly when all agents ended.
- The scheduler applies `--max-wait` and reports a stalled session instead of
  waiting forever.
- New goals are started by editing the manifest and running setup/watch again.
- Sequenta is implemented and verified, but the worktree bridge remains a
  separate integration wave and does not block this wave.
