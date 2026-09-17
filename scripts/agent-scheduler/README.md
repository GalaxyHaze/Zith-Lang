# Agent Scheduler

Small Python coordinator for the agent-worktrees workflow. It watches
`.awt/requests/<agent>.md`, reviews or merges accepted branches, and then
advances each agent through the task queue in `.awt/manifest.md`.

## Setup

The repo is the master checkout. Before running the queue, commit or stash
user changes in the master and create isolated agent worktrees:

```bash
$AWT_SKILL_DIR/scripts/awt setup /home/diogo/Zith 5
echo agent1 > /home/diogo/Zith/.awt/agent1/agent-id
echo agent2 > /home/diogo/Zith/.awt/agent2/agent-id
echo agent3 > /home/diogo/Zith/.awt/agent3/agent-id
echo agent4 > /home/diogo/Zith/.awt/agent4/agent-id
echo agent5 > /home/diogo/Zith/.awt/agent5/agent-id
```

`$AWT_SKILL_DIR` is typically
`/home/diogo/.byteask/skills/agent-worktrees`.

The active manifest is `.awt/manifest.md`. A versioned copy is kept at
`scripts/agent-scheduler/manifest.example.md`; after cleaning `.awt`, copy it
back:

```bash
cp scripts/agent-scheduler/manifest.example.md .awt/manifest.md
```

## Watch Output

At startup, `--watch` prints one compact line per agent so you can see the
initial state. After that, it prints only per-agent events when a new merge
request appears. Each event is also appended to
`.awt/events/<agent>.log`, so the terminal is not flooded with a full status
table on every poll.

## Start

Write the initial `TASK.md` files and clear any scheduler state:

```bash
python3 scripts/agent-scheduler/scheduler.py --require-clean --reset-queue
python3 scripts/agent-scheduler/scheduler.py --require-clean --setup
```

Run the coordinator in the master checkout:

```bash
python3 scripts/agent-scheduler/scheduler.py --require-clean --watch --merge
```

`--merge` reviews with `awt review` and then merges with `awt merge` before
dispatching the next task. Without `--merge`, the scheduler only reviews.

## Agent Wait

Each agent runs `agent-wait.sh` after requesting merge. It is the concrete
listening loop:

```bash
/home/diogo/Zith/scripts/agent-scheduler/agent-wait.sh /home/diogo/Zith agent1
```

The script watches `task-sequence:` in the worktree `TASK.md`. When the
scheduler writes a new sequence or the `end` marker, it prints the new content
and exits 0. The agent then reads the printed content: continue with the next
task or stop on `end`.

The script does not write `.awt/requests/` or `.awt/agent-state/`. The
scheduler is the single writer for agent lifecycle state.

## Queue And State

- `.awt/manifest.md`: agent sections with `Plan` bullets and `Task files`.
- `.awt/agent-state/<agent>.json`: authoritative per-agent lifecycle state
  (`running`, `waiting`, `blocked`, `ended`), written only by the scheduler.
- `.awt/scheduler-status.json`: compatibility mirror of scheduler state, reset
  with `--reset-queue`.
- `.awt/requests/<agent>.md`: signal that an agent finished and requested
  merge; consumed by `awt merge`.
- `.awt/events/<agent>.log`: per-agent log of new merge requests and merge
  outcomes written by the scheduler.
- `.awt/<agent>/TASK.md`: current task file written into each agent worktree.
- `task-sequence: N` at the top of `TASK.md` lets `agent-wait.sh` detect an
  advance even when the task body is identical to a previous dispatch.
- `# Status\nend\n` in a scheduler write is the terminal marker.
- `.awt/debts/<agent>.md`: debt reports collected into `.awt/debts-pending.md`
  and `.awt/board.md` at shutdown for human review.

Merge handling runs a review, agent worktree build/ctest, merge, consolidated
build/ctest, and only then advances the agent. A post-merge failure marks
that agent `blocked` with `failed_head`, `failed_stage`, and `merged_head`;
other agents continue.

## Useful Checks

```bash
python3 scripts/agent-scheduler/scheduler.py --check
python3 scripts/agent-scheduler/scheduler.py --check --json
```

## Safety

- `--reset-queue` removes `.awt/scheduler-status.json`, `.awt/agent-state/`,
  and local `.awt/requests` files, not worktrees, branches, or completed merge
  records.
- `--require-clean` refuses to start when the master has uncommitted changes.
- Scheduler never calls `awt cleanup`; cleanup is a separate deliberate action.
