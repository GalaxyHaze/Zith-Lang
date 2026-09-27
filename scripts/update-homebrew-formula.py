#!/usr/bin/env python3
"""Update the versioned source archive and SHA-256 in the Homebrew formula."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")


def replace_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise ValueError(f"could not update {label}")
    return updated


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--formula", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args()

    if not SEMVER.fullmatch(args.version):
        raise SystemExit(f"invalid semantic version: {args.version}")
    if not SHA256.fullmatch(args.sha256):
        raise SystemExit("sha256 must be a lowercase SHA-256 hash")

    try:
        text = args.formula.read_text(encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"cannot read formula: {error}") from error

    archive_url = (
        f"https://github.com/{args.repository}/archive/refs/tags/v{args.version}.tar.gz"
    )
    updated = replace_once(
        text, r'(?m)^  version "[^"]+"$', f'  version "{args.version}"', "version"
    )
    updated = replace_once(
        updated, r'(?m)^  url "[^"]+"$', f'  url "{archive_url}"', "url"
    )
    updated = replace_once(
        updated,
        r'(?m)^  sha256 "[0-9a-fA-F]{64}"$',
        f'  sha256 "{args.sha256}"',
        "sha256",
    )

    args.formula.write_text(updated, encoding="utf-8")
    print(f"updated Homebrew formula for v{args.version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
