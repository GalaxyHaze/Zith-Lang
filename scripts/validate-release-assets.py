#!/usr/bin/env python3
"""Validate the complete release asset set before distribution updates."""

from __future__ import annotations

import argparse
import re
import zipfile
from pathlib import Path


SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?$")


def expected_assets(version: str) -> tuple[str, ...]:
    return (
        "zithc-linux-amd64",
        "zithc-linux-arm64",
        "zithc-linux-amd64-musl",
        "zithc-linux-arm64-musl",
        "zithc-macos-amd64",
        "zithc-macos-arm64",
        "zithc-windows-amd64.exe",
        "zithc-windows-arm64.exe",
        f"zithc-stdlib-v{version}.tar.gz",
        f"zithc-stdlib-v{version}.zip",
        "zithc-wasm.zip",
        "zith-lsp-linux-amd64",
        "zith-lsp-linux-arm64",
        "zith-lsp-macos-amd64",
        "zith-lsp-macos-arm64",
        "zith-lsp-windows-amd64.exe",
        "zith-lsp-windows-arm64.exe",
    )


def fail(message: str) -> None:
    raise SystemExit(message)


def validate_wasm_archive(path: Path) -> None:
    try:
        with zipfile.ZipFile(path) as archive:
            members = set(archive.namelist())
            for required in ("zith-playground.wasm", "zith-stdlib.pack"):
                if required not in members:
                    fail(f"{path} is missing {required}")
                if archive.getinfo(required).file_size == 0:
                    fail(f"{path} contains an empty {required}")
    except (OSError, zipfile.BadZipFile) as error:
        fail(f"cannot inspect WASM archive {path}: {error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-dir", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    args = parser.parse_args()

    version = args.tag.removeprefix("v")
    if not SEMVER.fullmatch(version):
        fail(f"invalid release tag: {args.tag}")

    for name in expected_assets(version):
        path = args.release_dir / name
        if not path.is_file() or path.stat().st_size == 0:
            fail(f"missing or empty release asset: {path}")

    validate_wasm_archive(args.release_dir / "zithc-wasm.zip")
    print(f"release assets valid for v{version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
