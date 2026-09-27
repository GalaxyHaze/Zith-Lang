#!/usr/bin/env python3
"""Update Scoop release URLs and SHA-256 hashes from downloaded assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?$")
ASSETS = {
    "64bit": "zithc-windows-amd64.exe",
    "arm64": "zithc-windows-arm64.exe",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--release-dir", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    version = args.version.removeprefix("v")
    if not SEMVER.fullmatch(version):
        raise SystemExit(f"invalid release version: {args.version}")

    stdlib_name = f"zithc-stdlib-v{version}.zip"
    stdlib_path = args.release_dir / stdlib_name
    if not stdlib_path.is_file():
        raise SystemExit(f"missing release asset: {stdlib_path}")
    stdlib_hash = sha256(stdlib_path)

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    manifest["version"] = version
    base_url = f"https://github.com/{args.repository}/releases/download/v{version}"
    for architecture, binary_name in ASSETS.items():
        binary_path = args.release_dir / binary_name
        if not binary_path.is_file():
            raise SystemExit(f"missing release asset: {binary_path}")
        manifest["architecture"][architecture]["url"] = [
            f"{base_url}/{binary_name}",
            f"{base_url}/{stdlib_name}",
        ]
        manifest["architecture"][architecture]["hash"] = [
            sha256(binary_path),
            stdlib_hash,
        ]

    args.manifest.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )
    print(f"updated Scoop manifest for v{version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
