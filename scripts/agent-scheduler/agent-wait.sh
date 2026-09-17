#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: agent-wait.sh <repo> <agent> [interval]

Poll an agent worktree TASK.md until the scheduler advances the task or sends
the end marker. Prints the new TASK.md content and exits 0 on advance/end;
exits 2 on invalid input or unsupported TASK.md.
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -lt 2 ]]; then
    usage >&2
    exit 2
fi

REPO="${1%/}"
AGENT="$2"
INTERVAL="${3:-5}"

if ! [[ "$INTERVAL" =~ ^[1-9][0-9]*$ ]]; then
    echo "error: interval must be a positive integer" >&2
    exit 2
fi

TASK="$REPO/.awt/$AGENT/TASK.md"
if [[ ! -f "$TASK" ]]; then
    echo "error: no TASK.md for $AGENT at $TASK" >&2
    exit 2
fi

sequenta_status() {
    rg -q '^#+\s*Status' "$TASK" || return 1
    rg -qi '^end\s*$' "$TASK" || return 1
    return 0
}

task_sequence() {
    if [[ ! -f "$TASK" ]]; then
        echo ""
        return
    fi
    grep -E '^task-sequence:' "$TASK" | head -1 || true
}

CURRENT_SEQUENCE="$(task_sequence)"
if [[ -z "$CURRENT_SEQUENCE" ]]; then
    if sequenta_status; then
        cat "$TASK"
        exit 0
    fi
    echo "error: TASK.md has no task-sequence: marker (scheduler must write it)" >&2
    exit 2
fi

while true; do
    if [[ ! -f "$TASK" ]]; then
        echo "error: TASK.md disappeared for $AGENT" >&2
        exit 2
    fi
    if sequenta_status; then
        cat "$TASK"
        exit 0
    fi
    NEXT_SEQUENCE="$(task_sequence)"
    if [[ -n "$NEXT_SEQUENCE" && "$NEXT_SEQUENCE" != "$CURRENT_SEQUENCE" ]]; then
        cat "$TASK"
        exit 0
    fi
    sleep "$INTERVAL"
done
