# Agent Scheduler And Agent Wait

Summary: the scheduler now has an explicit agent-side polling script,
authoritative per-agent state under `.awt/agent-state/`, post-merge test
verification, debt collection, and a bounded `--max-wait`. The scheduler is the
single writer of agent lifecycle state; `agent-wait.sh` only reads `TASK.md`.

## Roles

- The master/scheduler owns review, merge, agent worktree build/ctest,
  consolidated build/ctest, queue advancement, block/end state, and debt
  collection.
- The agent owns implementing `TASK.md`, `awt checkin`, `awt request-merge`,
  and running `scripts/agent-scheduler/agent-wait.sh` after requesting merge.
- The agent must not touch `.awt/agent-state/` or `.awt/requests/` except
  through `awt request-merge`.

## Files

- `.awt/manifest.md`: source queue with `## agentN - title`, a `Plan` list, and
  optional `Task files`.
- `.awt/requests/<agent>.md`: merge request signal written by `awt
  request-merge` and consumed by `awt merge`.
- `.awt/<agent>/TASK.md`: task written by the scheduler. Every dispatch starts
  with `task-sequence: N`.
- `.awt/agent-state/<agent>.json`: authoritative lifecycle state. Valid statuses
  are `running`, `waiting`, `blocked`, `ended`.
- `.awt/scheduler-status.json`: compatibility mirror for older tooling; the
  scheduler still writes it after agent state changes.
- `.awt/events/<agent>.log`: scheduler event log.
- `.awt/debts/<agent>.md`: agent debt report, collected into `.awt/debts-pending.md`
  and `.awt/board.md`.

## Agent Wait Contract

`agent-wait.sh <repo> <agent> [interval]`:

- Polls `<repo>/.awt/<agent>/TASK.md` at a default 5 second interval.
- Requires a `task-sequence:` marker. It exits 2 if the marker is missing, so
  every dispatch must include the marker.
- Exits 0 and prints the whole file when the marker changes.
- Exits 0 and prints the whole file when `# Status` plus `end` is present.
- Never writes `.awt/requests/`, `.awt/agent-state/`, or the git index.

The scheduler increments `task_sequence` on every `write_pending` and
`write_end`. Treat the marker as the change signal, not task text equality:
identical task bodies across a re-dispatch would otherwise be invisible.

## Scheduler Loop

Production command:

```bash
python3 scripts/agent-scheduler/scheduler.py --watch --merge --interval 5
```

For each merge request:

1. Record the event with `seen_requests`.
2. Run `awt review` before merge.
3. Run agent worktree `cmake --build .awt/<agent>/build -j4`.
4. Run `ctest --test-dir .awt/<agent>/build --output-on-failure`.
5. Run `awt merge`.
6. Run master `cmake --build build -j4` and `ctest --test-dir build
   --output-on-failure`.
7. On success, add the active task to `done_tasks`, then dispatch the next task
   or write `end`.
8. On failure, mark only that agent `blocked`, keep metadata, and continue.

## Stop And Blocked States

- `ended` is written only when every manifest task is in `done_tasks`.
- `blocked` with `manifest_exhausted_without_done` is written when the manifest
  queue ends without the active task being done.
- Post-merge failure writes `status: blocked`, `failed_head`, `merged_head`,
  `failed_stage: build|ctest`, `acted_on: false`, and `reason`.
- Watch exits 0 when every agent is `ended` or `blocked`, generating
  `.awt/board.md`.
- `--max-wait` defaults to 600 seconds; timeout exits nonzero and leaves state
  for manual audit.

## Gotchas

- Legacy `.awt/scheduler-status.json` is loaded once for migration into
  `.awt/agent-state/`. New scheduler runs read agent state first, so do not
  reintroduce scheduler-status as authoritative.
- `--repo` must derive all `.awt` paths from the repo root, not only the
  manifest/requests paths.
- `--reset-queue` is destructive for scheduler state, requests, and agent state.
  Do not run it while agents are mid-task unless the wave is being abandoned.
- `awt merge` removes `.awt/requests/<agent>.md` after a successful merge. Do
  not manually recreate request files for merged work.
- Stale request inspections for `agent2` and `agent4` remain pending; audit
  their heads, branch commits, and duplicate-merge risk before accepting.

## Sequenta Bridge

Sequenta is implemented and isolated from this scheduler wave. It owns
`.sequenta/state.json`, written only through its CLI. The scheduler should not
couple to `sequenta` until a thin bridge maps manifest queues to modules and
reads readiness from `sequenta next`.
