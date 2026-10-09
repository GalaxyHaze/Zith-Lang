#!/usr/bin/env python3
"""Guard the documentation invariants established by the status cleanup.

The status document (`docs/impl-status.md`) is the single source of truth for
what the compiler implements. Two invariants keep the rest of the docs from
drifting away from it:

1. The numbered chapter docs point at the status document instead of restating
   a feature-status label (Working, Stub, Spec only, and so on).
2. The README pipeline and CLI tables match the status document.

The check is offline: it reads only files in the tree, so it can run in CI
without network access, next to the release-contract checks.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


STATUS_DOC = "docs/impl-status.md"
README = "README.md"
PIPELINE_HEADING = "Compiler Pipeline"
README_PIPELINE_HEADING = "Compilation Pipeline"
CLI_HEADING_STATUS = "CLI Commands"
CLI_HEADING_README = "CLI Reference"

STATUS_LEGEND_HEADING = "Status Legend"


class Inconsistency(ValueError):
    """Raised when a documentation invariant is violated."""


def section(lines: list[str], heading: str) -> list[str]:
    """Return the lines of a level-two section, excluding the heading."""
    start = None
    for index, line in enumerate(lines):
        if line.strip() == f"## {heading}":
            start = index + 1
            break
    if start is None:
        raise Inconsistency(f"missing section '## {heading}'")
    end = len(lines)
    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break
    return lines[start:end]


def tables(block: list[str]) -> list[list[list[str]]]:
    """Return every pipe table in a section as a list of rows of cells."""
    found: list[list[list[str]]] = []
    current: list[list[str]] = []
    for line in block:
        stripped = line.strip()
        if stripped.startswith("|") and stripped.endswith("|"):
            current.append([cell.strip() for cell in stripped.strip("|").split("|")])
            continue
        if current:
            found.append(current)
            current = []
    if current:
        found.append(current)
    return found


def data_rows(table: list[list[str]]) -> list[list[str]]:
    """Drop the header and separator rows from a parsed table."""
    rows = []
    for row in table:
        if all(set(cell) <= {"-", ":", " "} for cell in row):
            continue
        rows.append(row)
    return rows[1:] if rows else []


def strip_qualifier(text: str) -> str:
    """Drop a trailing parenthetical, e.g. 'Working (frozen)' -> 'Working'."""
    return re.sub(r"\s*\([^)]*\)\s*$", "", text).strip()


def normalize_status(cell: str) -> str:
    text = cell.replace("**", "").replace("`", "").strip()
    return strip_qualifier(text)


def normalize_stage(text: str) -> str:
    return re.sub(r"\s*/\s*", "/", text.strip())


def commands_in_cell(cell: str) -> list[str]:
    """Extract command names from a CLI table cell.

    A cell may hold one backticked command (`zithc build`) or several
    (`zithc deps add`, `deps remove`). Placeholders such as `<name>` are
    dropped so identity is the command path, not its arguments.
    """
    tokens = re.findall(r"`([^`]+)`", cell) or [cell]
    commands = []
    for token in tokens:
        token = token.strip()
        if token.startswith("zithc "):
            token = token[len("zithc ") :]
        token = re.sub(r"\s*[<\[].*$", "", token).strip()
        if token:
            commands.append(token)
    return commands


def status_vocabulary(status_lines: list[str]) -> set[str]:
    """The set of feature-status labels that a chapter must not repeat."""
    labels: set[str] = set()
    for row in data_rows(tables(section(status_lines, STATUS_LEGEND_HEADING))[0]):
        labels.add(strip_qualifier(row[0].replace("**", "").strip()))
    for heading in (PIPELINE_HEADING, CLI_HEADING_STATUS):
        for row in data_rows(tables(section(status_lines, heading))[0]):
            labels.add(normalize_status(row[1]))
    labels.discard("")
    return labels


def check_chapters(root: Path, vocabulary: set[str]) -> None:
    chapters = sorted((root / "docs").glob("[0-9][0-9]-*.md"))
    if not chapters:
        raise Inconsistency("no numbered chapter docs found under docs/")
    for chapter in chapters:
        text = chapter.read_text(encoding="utf-8")
        for match in re.finditer(r"\*\*([^*]+)\*\*", text):
            label = strip_qualifier(match.group(1).strip())
            if label in vocabulary:
                line = text.count("\n", 0, match.start()) + 1
                raise Inconsistency(
                    f"{chapter.relative_to(root)}:{line} repeats the feature-status "
                    f"label '{label}'; point at {STATUS_DOC} instead"
                )


def read_pipeline_stages(status_lines: list[str]) -> list[str]:
    rows = data_rows(tables(section(status_lines, PIPELINE_HEADING))[0])
    return [normalize_stage(row[0]) for row in rows]


def read_cli_table(lines: list[str], heading: str) -> dict[str, str]:
    table = tables(section(lines, heading))[0]
    header = [cell.replace("**", "").strip().lower() for cell in table[0]]
    if "status" not in header:
        raise Inconsistency(f"'{heading}' table has no Status column")
    status_column = header.index("status")
    rows = data_rows(table)
    commands: dict[str, str] = {}
    for row in rows:
        for command in commands_in_cell(row[0]):
            commands[command] = normalize_status(row[status_column])
    return commands


def read_readme_pipeline(readme_lines: list[str]) -> tuple[list[str], list[str]]:
    block = section(readme_lines, README_PIPELINE_HEADING)
    sequence: list[str] = []
    in_fence = False
    for line in block:
        if line.strip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence and "->" in line:
            sequence.extend(normalize_stage(part) for part in line.split("->") if part.strip())
    if not sequence:
        raise Inconsistency(f"{README} has no pipeline stage sequence")
    stage_table = [normalize_stage(row[0].replace("`", "")) for row in
                   data_rows(tables(block)[0])]
    return sequence, stage_table


def check_pipeline(root: Path, status_lines: list[str]) -> None:
    status_stages = read_pipeline_stages(status_lines)
    # The status table also carries stdlib status rows; the README pipeline
    # enumerates only the compiler stages, so compare against those.
    expected = [stage for stage in status_stages if not stage.startswith("Stdlib")]
    readme_lines = (root / README).read_text(encoding="utf-8").splitlines()
    sequence, stage_table = read_readme_pipeline(readme_lines)
    if sequence != stage_table:
        raise Inconsistency(
            f"{README} pipeline diagram and stage table disagree: "
            f"{sequence} != {stage_table}"
        )
    if sequence != expected:
        raise Inconsistency(
            f"{README} pipeline stages do not match {STATUS_DOC}: "
            f"{sequence} != {expected}"
        )


def check_cli(root: Path, status_lines: list[str]) -> None:
    status_commands = read_cli_table(status_lines, CLI_HEADING_STATUS)
    readme_lines = (root / README).read_text(encoding="utf-8").splitlines()
    readme_commands = read_cli_table(readme_lines, CLI_HEADING_README)
    missing = sorted(set(status_commands) - set(readme_commands))
    extra = sorted(set(readme_commands) - set(status_commands))
    if missing or extra:
        raise Inconsistency(
            f"{README} CLI table does not match {STATUS_DOC}: "
            f"missing {missing}, unexpected {extra}"
        )
    for command, status in sorted(readme_commands.items()):
        if status_commands[command] != status:
            raise Inconsistency(
                f"{README} lists '{command}' as '{status}' but {STATUS_DOC} "
                f"lists '{status_commands[command]}'"
            )


def run(root: Path) -> None:
    status_lines = (root / STATUS_DOC).read_text(encoding="utf-8").splitlines()
    check_chapters(root, status_vocabulary(status_lines))
    check_pipeline(root, status_lines)
    check_cli(root, status_lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="repository root to check (defaults to the script's repository)",
    )
    args = parser.parse_args()
    try:
        run(args.root.resolve())
    except Inconsistency as error:
        print(f"docs consistency error: {error}", file=sys.stderr)
        return 1
    print(f"docs consistency valid: {args.root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
