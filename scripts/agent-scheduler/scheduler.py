#!/usr/bin/env python3
"""Polling coordinator for agent worktrees.

The scheduler watches `.awt/requests/<agent>.md`, reviews and optionally merges
agent branches through the `agent-worktrees` awt scripts, then advances each
agent through a task queue from `.awt/manifest.md`.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


AWT_SKILL_DIR = Path(os.environ.get("AWT_SKILL_DIR", Path.home() / ".byteask" / "skills" / "agent-worktrees"))
AWT_DIR = AWT_SKILL_DIR / "scripts"
AWT = AWT_DIR / "awt"
REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / ".awt" / "manifest.md"
REQUESTS = REPO / ".awt" / "requests"
TASKS = REPO / ".awt" / "tasks"
STATUS = REPO / ".awt" / "scheduler-status.json"

AGENT_RE = re.compile(r"^##\s+(?P<agent>agent\d+)\s+-\s+(?P<title>.*)$")


@dataclass
class AgentPlan:
    agent: str
    title: str
    tasks: list[str]
    task_files: list[str] = field(default_factory=list)


@dataclass
class AgentState:
    done_tasks: list[str] = field(default_factory=list)
    active_task: str | None = None
    plan: dict[str, object] = field(default_factory=dict)


def log(message: str) -> None:
    print(f"[scheduler] {message}", flush=True)


def run(command: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        cwd=str(REPO),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def clean_task_title(title: str) -> str:
    return re.sub(r"^#+\s*", "", title).strip()


def parse_manifest(path: Path = MANIFEST) -> list[AgentPlan]:
    if not path.is_file():
        return []

    plans: list[AgentPlan] = []
    current: AgentPlan | None = None
    in_plan_lines = False
    in_task_file_lines = False
    plan_lines: list[str] = []
    file_lines: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = AGENT_RE.match(line)
        if match:
            if current is not None:
                current.tasks = list(plan_task_items("\n".join(plan_lines)))
                current.task_files = [x.strip() for x in file_lines if x.strip()]
            current = AgentPlan(agent=match.group("agent"), title=match.group("title"), tasks=[])
            plans.append(current)
            in_plan_lines = False
            in_task_file_lines = False
            plan_lines = []
            file_lines = []
            continue
        if current is None or line.startswith("## "):
            continue
        if line.strip().lower() in {"task files", "task files:", "files"}:
            in_task_file_lines = True
            in_plan_lines = False
            file_lines = []
            continue
        if line.strip().lower() in {"plan", "tasks", "task list", "queue"}:
            in_plan_lines = True
            in_task_file_lines = False
            plan_lines = []
            continue
        if in_task_file_lines:
            if line.startswith(("- ", "* ")):
                file_lines.append(line)
            else:
                in_task_file_lines = False
        elif in_plan_lines:
            plan_lines.append(line)

    if current is not None:
        current.tasks = list(plan_task_items("\n".join(plan_lines)))
        current.task_files = [x.strip() for x in file_lines if x.strip()]
    return plans


def plan_task_items(text: str) -> Iterable[str]:
    for raw in text.splitlines():
        item = clean_task_title(raw.lstrip("-*1234567890. ")).strip()
        if item and not item.lower().startswith(("acceptance", "verification", "depends on")):
            yield item


def load_state() -> dict[str, AgentState]:
    if not STATUS.is_file():
        return {}
    try:
        raw = json.loads(STATUS.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    states: dict[str, AgentState] = {}
    for agent, value in raw.items():
        if not isinstance(value, dict):
            continue
        done = value.get("done_tasks", [])
        states[agent] = AgentState(
            done_tasks=[str(x) for x in done] if isinstance(done, list) else [],
            active_task=value.get("active_task"),
            plan=value.get("plan", {}) if isinstance(value.get("plan"), dict) else {},
        )
    return states


def save_state(states: dict[str, AgentState]) -> None:
    STATUS.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        agent: {
            "done_tasks": state.done_tasks,
            "active_task": state.active_task,
            "plan": state.plan,
        }
        for agent, state in sorted(states.items())
    }
    STATUS.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def plan_brief(plan: AgentPlan) -> str:
    return plan.tasks[0] if plan.tasks else "no pending task"


def current_task(states: dict[str, AgentState], plan: AgentPlan) -> str | None:
    state = states.get(plan.agent)
    if state is None:
        return plan.tasks[0] if plan.tasks else None
    task_ids = [clean_task_title(x) for x in plan.tasks]
    done_ids = {clean_task_title(x) for x in state.done_tasks}
    active = clean_task_title(state.active_task) if state.active_task else ""
    if active and active in task_ids and active not in done_ids:
        return state.active_task
    for task in plan.tasks:
        clean = clean_task_title(task)
        if clean not in done_ids:
            return task
    return None


def merge_requested(agent: str) -> bool:
    return (REQUESTS / f"{agent}.md").is_file()


def write_task_file(agent: str, content: str) -> None:
    task_path = REPO / ".awt" / agent / "TASK.md"
    task_path.parent.mkdir(parents=True, exist_ok=True)
    task_path.write_text(content, encoding="utf-8")
    log(f"wrote task file for {agent}: {task_path}")


def write_end(agent: str) -> None:
    write_task_file(
        agent,
        f"# {agent}: end\n\n## Status\n\nend\n\n## Message\n\nAll assigned tasks are complete or the master stopped this agent.\n",
    )


def write_pending(agent: str, plan: AgentPlan, task: str, index: int, total: int) -> None:
    task_path = REPO / ".awt" / agent / "TASK.md"
    if index < len(plan.task_files):
        raw_source = plan.task_files[index].lstrip("-*").strip()
        source = Path(raw_source)
        if not source.is_absolute():
            source = REPO / source
        if source.is_file():
            content = source.read_text(encoding="utf-8")
            replacements = {
                "{AGENT}": agent,
                "{TASK}": clean_task_title(task),
                "{REPO}": str(REPO),
                "{AWT_SKILL_DIR}": str(AWT_SKILL_DIR),
                "{INDEX}": str(index + 1),
                "{TOTAL}": str(total),
                "{TASK_PATH}": str(task_path),
            }
            for token, value in replacements.items():
                content = content.replace(token, value)
            write_task_file(agent, content)
            log(f"dispatched task file {source} for {agent}")
            return
    content = "\n".join(
        [
                f"# {agent}: {task}",
                "",
                f"## Goal",
                "",
                "Advance the current Zith-- task below. The compiler main is the Zith--",
                "subset documented in docs/Zith--.md and docs/impl-status.md; full-Zith",
                "features outside that subset are out of scope.",
                "",
                "## Context",
                "",
                f"- Plan: {plan.title}",
                f"- Task {index + 1} of {total} in this agent queue.",
                f"- Worktree: {REPO}/.awt/{agent} (branch awt/{agent})",
                "- Do not touch files owned by other agent tasks. If a conflict appears,",
                "  stop, report it, and request merge without resolving the other agent's work.",
                "",
                "## Scope",
                "",
                task,
                "",
                "## Acceptance Criteria",
                "",
                "- The described Zith-- behavior or refactor is implemented or documented.",
                "- Public compiler behavior outside the task scope is unchanged.",
                "- No debug prints; use ZITH_DEBUG_PRINT only when explicitly needed.",
                "",
                "## Verification",
                "",
                "Run the focused project tests, then the full suite when practical:",
                "",
                "    cmake --build {REPO}/build -j4",
                "    ctest --test-dir {REPO}/build --output-on-failure",
                "",
                "After verification:",
                "",
                "    cd {REPO}/.awt/{agent}",
                "    {AWT_SKILL_DIR}/scripts/awt checkin {REPO} \"{agent}: {task}\"",
                "    {AWT_SKILL_DIR}/scripts/awt request-merge {REPO} \"{agent}: {task}\"",
                "",
                "## End Of Task",
                "",
                "When this item is complete, request merge. The master scheduler will",
                "review and either advance this worktree to the next task or write",
                "If the scheduler writes `# Status` with `end` below it, stop",
                "and do not request further work.",
                "",
            ]
        )
    replacements = {
        "{AGENT}": agent,
        "{TASK}": clean_task_title(task),
        "{REPO}": str(REPO),
        "{AWT_SKILL_DIR}": str(AWT_SKILL_DIR),
        "{INDEX}": str(index + 1),
        "{TOTAL}": str(total),
        "{TASK_PATH}": str(task_path),
    }
    for token, value in replacements.items():
        content = content.replace(token, value)
    write_task_file(agent, content)


def review_agent(agent: str) -> None:
    result = run([str(AWT), "review", str(REPO), agent], check=False)
    if result.returncode != 0:
        log(f"review failed for {agent}: {result.stderr.strip()}")
    else:
        log(f"review ok for {agent}: {result.stdout.strip()}")


def merge_agent(agent: str) -> None:
    review_agent(agent)
    log(f"merging {agent}")
    result = run([str(AWT), "merge", str(REPO), agent], check=False)
    if result.returncode != 0:
        log(f"merge failed for {agent}: {result.stderr.strip()}")


def advance_agent(agent: str, states: dict[str, AgentState], plan: AgentPlan) -> None:
    state = states.setdefault(agent, AgentState())
    task = current_task(states, plan)
    state.active_task = None
    if task is None:
        write_end(agent)
        log(f"{agent}: no pending task; wrote end")
        return
    state.active_task = task
    write_pending(agent, plan, task, plan.tasks.index(task), len(plan.tasks))
    if state.plan.get("current") != clean_task_title(task):
        state.plan["current"] = clean_task_title(task)
    log(f"{agent}: dispatched '{task}'")


def setup_queue(states: dict[str, AgentState], plans: list[AgentPlan]) -> None:
    for plan in plans:
        if plan.agent not in states:
            states[plan.agent] = AgentState()
        if not states[plan.agent].active_task:
            advance_agent(plan.agent, states, plan)
    save_state(states)
    log(f"queue ready for {len(plans)} agents")


def machine_name() -> str:
    try:
        host = os.uname().nodename
        return host if host else "host"
    except (AttributeError, OSError):
        return "host"


def clear_local_agent(agent: str) -> None:
    for path in (REQUESTS / f"{agent}.md", TASKS / f"{agent}.md"):
        if path.is_file():
            path.unlink()
    merged = REPO / ".awt" / "merged" / "agents.log"
    if merged.is_file() and agent in merged.read_text(encoding="utf-8", errors="replace"):
        log(f"{agent}: already in merge ledger; keeping existing merge record")


def reconcile(states: dict[str, AgentState], plans: list[AgentPlan], *, merge: bool) -> None:
    for plan in plans:
        agent = plan.agent
        state = states.setdefault(agent, AgentState())
        active = clean_task_title(state.active_task or "")
        if not active:
            continue
        if merge_requested(agent):
            log(f"{agent}: merge request seen")
            if merge:
                merge_agent(agent)
                merged = (REPO / ".awt" / "merged" / "agents.log").is_file() and agent in (
                    REPO / ".awt" / "merged" / "agents.log"
                ).read_text(encoding="utf-8", errors="replace")
                if not merged and not (REQUESTS / f"{agent}.md").exists():
                    merged = True
                if merged:
                    state.done_tasks.append(state.active_task)
                    advance_agent(agent, states, plan)
                else:
                    log(f"{agent}: merge did not complete; keeping task active")
            else:
                review_agent(agent)
    save_state(states)


def status_text(states: dict[str, AgentState], plans: list[AgentPlan]) -> str:
    lines = ["agent,current,queue_tasks,done_tasks,merge_requested"]
    for plan in plans:
        state = states.get(plan.agent, AgentState())
        current = clean_task_title(state.active_task or plan_brief(plan))
        lines.append(
            ",".join(
                [
                    plan.agent,
                    current,
                    str(len(plan.tasks)),
                    str(len(state.done_tasks)),
                    "yes" if merge_requested(plan.agent) else "no",
                ]
            )
        )
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Poll agent worktrees and dispatch the next task or end.",
    )
    parser.add_argument(
        "--setup",
        action="store_true",
        help="load .awt/manifest.md and write initial TASK.md files without waiting",
    )
    parser.add_argument(
        "--watch",
        action="store_true",
        help="poll .awt/requests/ until every agent is done",
    )
    parser.add_argument(
        "--interval",
        type=int,
        default=5,
        help="poll interval in seconds (default 5)",
    )
    parser.add_argument(
        "--merge",
        action="store_true",
        help="review and merge accepted agent branches before dispatching next task",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="print the current queue and exit without changing files",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="print status as JSON",
    )
    parser.add_argument(
        "--repo",
        metavar="PATH",
        dest="repo",
        help="override scheduler repo path (default: this checkout)",
    )
    parser.add_argument(
        "--manifest",
        metavar="PATH",
        help="override manifest path",
    )
    parser.add_argument(
        "--agent",
        metavar="AGENT",
        help="limit operations to one agent id, e.g. agent1",
    )
    parser.add_argument(
        "--clear-agent",
        metavar="AGENT",
        help="remove local scheduler state for one agent id",
    )
    parser.add_argument(
        "--reset-queue",
        action="store_true",
        help="clear local scheduler state, keeping agent worktrees untouched",
    )
    parser.add_argument(
        "--require-clean",
        action="store_true",
        help="refuse setup/watch until master git worktree is clean",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    global REPO, MANIFEST, REQUESTS, TASKS, STATUS
    if args.repo:
        REPO = Path(args.repo).resolve()
        MANIFEST = REPO / ".awt" / "manifest.md"
        REQUESTS = REPO / ".awt" / "requests"
        TASKS = REPO / ".awt" / "tasks"
        STATUS = REPO / ".awt" / "scheduler-status.json"
    if args.manifest:
        MANIFEST = Path(args.manifest).resolve()
    if not shutil.which("git"):
        log("error: git is required")
        return 2
    if not AWT.is_file() and not shutil.which("awt"):
        log(f"error: agent-worktrees awt script not found at {AWT}")
        return 2

    if args.clear_agent:
        states = load_state()
        states.pop(args.clear_agent, None)
        save_state(states)
        clear_local_agent(args.clear_agent)
        log(f"cleared scheduler state for {args.clear_agent}")
        return 0

    if args.reset_queue:
        if STATUS.is_file():
            STATUS.unlink()
        for path in REQUESTS.glob("*.md"):
            path.unlink()
        log("reset local scheduler queue and pending merge-request files")
        return 0

    if args.require_clean:
        dirty = run(["git", "status", "--porcelain"], check=False)
        if dirty.stdout.strip():
            log(f"error: master worktree is not clean; commit or stash these changes:\n{dirty.stdout}")
            return 2

    plans = parse_manifest(MANIFEST)
    if args.check and not plans:
        # A check-only smoke run is valid while no queue exists yet.
        print("no agent queue")
        return 0
    if not plans:
        log(f"error: no agent sections in {MANIFEST}")
        return 2
    if args.agent:
        plans = [plan for plan in plans if plan.agent == args.agent]
        if not plans:
            log(f"error: no manifest section for {args.agent}")
            return 2

    states = load_state()
    if args.check:
        if args.json:
            print(
                json.dumps(
                    {
                        "machine": machine_name(),
                        "agents": [
                            {
                                "agent": plan.agent,
                                "current": clean_task_title(
                                    states.get(plan.agent, AgentState()).active_task
                                    or plan_brief(plan)
                                ),
                                "tasks": plan.tasks,
                                "done": states.get(plan.agent, AgentState()).done_tasks,
                                "merge_requested": merge_requested(plan.agent),
                            }
                            for plan in plans
                        ],
                    },
                    indent=2,
                    sort_keys=True,
                )
            )
        else:
            print(status_text(states, plans))
        return 0

    if args.setup:
        setup_queue(states, plans)
        return 0

    if args.watch:
        log(f"watching {REQUESTS} every {args.interval}s; Ctrl-C to stop")
        try:
            while True:
                states = load_state()
                reconcile(states, plans, merge=args.merge)
                print(status_text(states, plans))
                if all(
                    clean_task_title(states.get(plan.agent, AgentState()).active_task or "")
                    in {
                        clean_task_title(x)
                        for x in states.get(plan.agent, AgentState()).done_tasks
                    }
                    or current_task(states, plan) is None
                    for plan in plans
                ):
                    log("all agents done")
                    return 0
                time.sleep(args.interval)
        except KeyboardInterrupt:
            log("stopped by user")
            return 0

    log("provide --setup, --watch, or --check")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
