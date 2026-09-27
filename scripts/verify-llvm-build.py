#!/usr/bin/env python3
"""Verify that a configured native build retained the required LLVM backend."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


CACHE_ENTRIES = {
    "ZITH_HAS_LLVM": re.compile(r"^ZITH_HAS_LLVM:BOOL=(ON|OFF)$", re.MULTILINE),
    "ZITH_REQUIRE_LLVM": re.compile(r"^ZITH_REQUIRE_LLVM:BOOL=(ON|OFF)$", re.MULTILINE),
    "ZITH_DETECTED_LLVM_VERSION": re.compile(
        r"^ZITH_DETECTED_LLVM_VERSION:(?:INTERNAL|STRING)=([0-9]+(?:\.[0-9]+)*)$",
        re.MULTILINE,
    ),
}
TARGET_ENTRY = re.compile(
    r"^CMAKE_CXX_COMPILER_TARGET:(?:STRING|UNINITIALIZED)=(.+)$", re.MULTILINE
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument(
        "--expected-target",
        help="Require CMake to have configured this C++ compiler target triple",
    )
    args = parser.parse_args()

    cache_path = args.build_dir / "CMakeCache.txt"
    try:
        cache = cache_path.read_text(encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"cannot read {cache_path}: {error}") from error

    values = {}
    for name, pattern in CACHE_ENTRIES.items():
        match = pattern.search(cache)
        if not match:
            raise SystemExit(f"{name} is missing from {cache_path}")
        values[name] = match.group(1)

    if values["ZITH_HAS_LLVM"] != "ON":
        raise SystemExit("configured build has ZITH_HAS_LLVM=OFF")
    if values["ZITH_REQUIRE_LLVM"] != "ON":
        raise SystemExit("configured build has ZITH_REQUIRE_LLVM=OFF")

    major = int(values["ZITH_DETECTED_LLVM_VERSION"].split(".", maxsplit=1)[0])
    if major < 18:
        raise SystemExit(
            f"configured build uses LLVM {values['ZITH_DETECTED_LLVM_VERSION']}, "
            "but LLVM 18+ is required"
        )

    if args.expected_target:
        target_match = TARGET_ENTRY.search(cache)
        if not target_match:
            raise SystemExit(
                f"CMAKE_CXX_COMPILER_TARGET is missing from {cache_path}"
            )
        target = target_match.group(1).strip()
        if target != args.expected_target:
            raise SystemExit(
                f"configured build targets {target!r}, "
                f"expected {args.expected_target!r}"
            )

    print(
        "LLVM backend verified: "
        f"LLVM {values['ZITH_DETECTED_LLVM_VERSION']} "
        "(ZITH_HAS_LLVM=ON, ZITH_REQUIRE_LLVM=ON)"
        + (f", target={args.expected_target}" if args.expected_target else "")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
