#!/usr/bin/env python3
"""Exercise release metadata generation without contacting GitHub."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"
MANIFEST = ROOT / ".github/scoop/bucket/zithc.json"
FORMULA = ROOT / ".github/homebrew/zithc.rb"
VERSION = "9.8.7"
REPOSITORY = "GalaxyHaze/Zith-Lang"


def run(*args: str, cwd: Path | None = None) -> None:
    subprocess.run(
        [sys.executable, *args],
        cwd=cwd or ROOT,
        check=True,
        env={**os.environ, "PYTHONPATH": str(SCRIPTS)},
    )


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="zith-release-contract-") as raw_dir:
        directory = Path(raw_dir)
        release_assets = (
            "zithc-linux-amd64",
            "zithc-linux-arm64",
            "zithc-linux-amd64-musl",
            "zithc-linux-arm64-musl",
            "zithc-macos-amd64",
            "zithc-macos-arm64",
            "zithc-windows-amd64.exe",
            "zithc-windows-arm64.exe",
            "zith-lsp-linux-amd64",
            "zith-lsp-linux-arm64",
            "zith-lsp-macos-amd64",
            "zith-lsp-macos-arm64",
            "zith-lsp-windows-amd64.exe",
            "zith-lsp-windows-arm64.exe",
        )
        for name in release_assets:
            (directory / name).write_bytes(b"release asset\n")
        with zipfile.ZipFile(directory / "zithc-wasm.zip", "w") as archive:
            archive.writestr("zith-playground.wasm", b"wasm\n")
            archive.writestr("zith-stdlib.pack", b"stdlib\n")
        (directory / f"zithc-stdlib-v{VERSION}.tar.gz").write_bytes(b"stdlib tar\n")
        (directory / f"zithc-stdlib-v{VERSION}.zip").write_bytes(b"stdlib zip\n")
        build_dir = directory / "build"
        build_dir.mkdir()
        (build_dir / "CMakeCache.txt").write_text(
            "\n".join(
                (
                    "ZITH_HAS_LLVM:BOOL=ON",
                    "ZITH_REQUIRE_LLVM:BOOL=ON",
                    "ZITH_DETECTED_LLVM_VERSION:INTERNAL=18.1.8",
                    "CMAKE_CXX_COMPILER_TARGET:STRING=x86_64-linux-musl",
                )
            )
            + "\n",
            encoding="utf-8",
        )
        run(
            str(SCRIPTS / "verify-llvm-build.py"),
            "--build-dir",
            str(build_dir),
            "--expected-target",
            "x86_64-linux-musl",
        )
        mismatch = subprocess.run(
            [
                sys.executable,
                str(SCRIPTS / "verify-llvm-build.py"),
                "--build-dir",
                str(build_dir),
                "--expected-target",
                "aarch64-linux-musl",
            ],
            cwd=ROOT,
            env={**os.environ, "PYTHONPATH": str(SCRIPTS)},
            check=False,
            capture_output=True,
            text=True,
        )
        if mismatch.returncode == 0 or "expected 'aarch64-linux-musl'" not in (
            mismatch.stdout + mismatch.stderr
        ):
            raise AssertionError("LLVM target verification accepted a mismatched target")
        run(
            str(SCRIPTS / "validate-release-assets.py"),
            "--release-dir",
            str(directory),
            "--tag",
            f"v{VERSION}",
        )

        for name, content in (
            ("zithc-windows-amd64.exe", b"amd64 test asset\n"),
            ("zithc-windows-arm64.exe", b"arm64 test asset\n"),
            ("zithc-stdlib-v" + VERSION + ".zip", b"stdlib test asset\n"),
        ):
            (directory / name).write_bytes(content)

        manifest = directory / "zithc.json"
        formula = directory / "zithc.rb"
        shutil.copyfile(MANIFEST, manifest)
        shutil.copyfile(FORMULA, formula)

        run(
            str(SCRIPTS / "update-scoop-manifest.py"),
            "--manifest",
            str(manifest),
            "--release-dir",
            str(directory),
            "--repository",
            REPOSITORY,
            "--version",
            VERSION,
        )
        run(
            str(SCRIPTS / "update-homebrew-formula.py"),
            "--formula",
            str(formula),
            "--repository",
            REPOSITORY,
            "--version",
            VERSION,
            "--sha256",
            "0123456789abcdef" * 4,
        )
        run(
            str(SCRIPTS / "validate-release-contract.py"),
            "--manifest",
            str(manifest),
            "--workflow-dir",
            str(ROOT / ".github/workflows"),
        )

        metadata = json.loads(manifest.read_text(encoding="utf-8"))
        stdlib_hash = digest(directory / f"zithc-stdlib-v{VERSION}.zip")
        for architecture, binary in (
            ("64bit", "zithc-windows-amd64.exe"),
            ("arm64", "zithc-windows-arm64.exe"),
        ):
            expected = [digest(directory / binary), stdlib_hash]
            if metadata["architecture"][architecture]["hash"] != expected:
                raise AssertionError(
                    f"unexpected hashes for {architecture}: "
                    f"{metadata['architecture'][architecture]['hash']}"
                )

        formula_text = formula.read_text(encoding="utf-8")
        expected_url = (
            f"https://github.com/{REPOSITORY}/archive/refs/tags/v{VERSION}.tar.gz"
        )
        for expected in (f'version "{VERSION}"', f'url "{expected_url}"'):
            if expected not in formula_text:
                raise AssertionError(f"missing Homebrew metadata: {expected}")
        if 'sha256 "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"' not in formula_text:
            raise AssertionError("missing Homebrew SHA-256")

    print("release contract integration test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
