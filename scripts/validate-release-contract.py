#!/usr/bin/env python3
"""Validate release metadata consumed by installers and package managers."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from urllib.parse import urlparse


SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
ARCHITECTURES = {
    "64bit": "zithc-windows-amd64.exe",
    "arm64": "zithc-windows-arm64.exe",
}
RELEASE_ASSETS = (
    "zithc-linux-amd64",
    "zithc-linux-arm64",
    "zithc-linux-amd64-musl",
    "zithc-linux-arm64-musl",
    "zithc-macos-amd64",
    "zithc-macos-arm64",
    "zithc-windows-amd64.exe",
    "zithc-windows-arm64.exe",
    "zithc-stdlib-${TAG}.tar.gz",
    "zithc-stdlib-${TAG}.zip",
    "zithc-wasm.zip",
    "zith-lsp-linux-amd64",
    "zith-lsp-linux-arm64",
    "zith-lsp-macos-amd64",
    "zith-lsp-macos-arm64",
    "zith-lsp-windows-amd64.exe",
    "zith-lsp-windows-arm64.exe",
)


def fail(message: str) -> None:
    raise ValueError(message)


def validate_url(url: object, version: str, expected_asset: str) -> None:
    if not isinstance(url, str):
        fail(f"URL for {expected_asset} is not a string")
    parsed = urlparse(url)
    if parsed.scheme != "https" or parsed.netloc != "github.com":
        fail(f"URL for {expected_asset} is not a GitHub HTTPS URL: {url}")
    expected_suffix = f"/releases/download/v{version}/{expected_asset}"
    if not parsed.path.endswith(expected_suffix):
        fail(f"URL for {expected_asset} does not target v{version}: {url}")


def validate_hash(value: object, label: str, allow_empty: bool) -> None:
    if value == "" and allow_empty:
        return
    if not isinstance(value, str) or not SHA256.fullmatch(value):
        fail(f"{label} must be a lowercase SHA-256 hash")


def validate_manifest(path: Path, allow_empty_hashes: bool) -> None:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read JSON manifest {path}: {error}")

    version = manifest.get("version")
    if not isinstance(version, str) or not SEMVER.fullmatch(version):
        fail("manifest version must be semantic version text without a leading v")

    architectures = manifest.get("architecture")
    if not isinstance(architectures, dict):
        fail("manifest architecture must be an object")

    for architecture, binary_name in ARCHITECTURES.items():
        entry = architectures.get(architecture)
        if not isinstance(entry, dict):
            fail(f"missing architecture entry: {architecture}")
        urls = entry.get("url")
        hashes = entry.get("hash")
        if not isinstance(urls, list) or len(urls) != 2:
            fail(f"{architecture}.url must contain the binary and stdlib URLs")
        if not isinstance(hashes, list) or len(hashes) != 2:
            fail(f"{architecture}.hash must contain binary and stdlib hashes")
        validate_url(urls[0], version, binary_name)
        validate_url(urls[1], version, f"zithc-stdlib-v{version}.zip")
        validate_hash(hashes[0], f"{architecture}.hash[0]", allow_empty_hashes)
        validate_hash(hashes[1], f"{architecture}.hash[1]", allow_empty_hashes)

    autoupdate = manifest.get("autoupdate", {}).get("architecture", {})
    for architecture, binary_name in ARCHITECTURES.items():
        urls = autoupdate.get(architecture, {}).get("url")
        if not isinstance(urls, list) or len(urls) != 2:
            fail(f"autoupdate entry is incomplete for {architecture}")
        if f"zithc-stdlib-v$version.zip" not in urls[1]:
            fail(f"autoupdate stdlib URL is invalid for {architecture}")
        if binary_name not in urls[0] or "v$version" not in urls[0]:
            fail(f"autoupdate binary URL is invalid for {architecture}")

    installer = manifest.get("installer", {}).get("script", [])
    installer_text = "\n".join(installer) if isinstance(installer, list) else ""
    if "Expand-Archive" not in installer_text:
        fail("installer must extract the stdlib archive")
    if "Remove-Item" not in installer_text:
        fail("installer must remove stale stdlib files")


def validate_workflows(workflow_dir: Path) -> None:
    release_page = workflow_dir / "create-new-release.yml"
    artifact_workflow = workflow_dir / "build-artifact.yml"
    ci_workflow = workflow_dir / "ci.yml"
    repair_workflow = workflow_dir / "update-package.yml"
    try:
        release_text = release_page.read_text(encoding="utf-8")
        artifact_text = artifact_workflow.read_text(encoding="utf-8")
        ci_text = ci_workflow.read_text(encoding="utf-8")
        repair_text = repair_workflow.read_text(encoding="utf-8")
        formula_text = (workflow_dir.parent / "homebrew/zithc.rb").read_text(
            encoding="utf-8"
        )
    except OSError as error:
        fail(f"cannot read release workflow: {error}")

    if "/master/" in release_text:
        fail(f"{release_page} contains an obsolete master-branch installer URL")
    expected_installers = (
        "/main/scripts/install.sh",
        "/main/scripts/install.ps1",
        "/main/scripts/install-wasm.sh",
    )
    for installer_path in expected_installers:
        if installer_path not in release_text:
            fail(f"{release_page} is missing the {installer_path} URL")
    if release_text.count("/main/") != 4:
        fail(f"{release_page} must publish four installer URLs from main")
    if "version: ${{ steps.tag.outputs.version }}" not in release_text:
        fail(f"{release_page} does not expose the normalized release version")
    if "tag_name: v${{ needs.tag-and-release.outputs.version }}" not in release_text:
        fail(f"{release_page} passes the raw version input to build-artifact")
    if "draft: true" not in release_text:
        fail(f"{release_page} exposes the release before verification gates pass")
    if "draft: false" not in release_text.split("secrets: inherit", 1)[0]:
        fail(f"{release_page} does not request final publication after verification")
    for installer in (
        Path("scripts/install.sh"),
        Path("scripts/install.ps1"),
        Path("scripts/install-wasm.sh"),
    ):
        installer_path = workflow_dir.parent.parent / installer
        try:
            installer_text = installer_path.read_text(encoding="utf-8")
        except OSError as error:
            fail(f"cannot read installer {installer_path}: {error}")
        if "GalaxyHaze/Zith-Lang" not in installer_text:
            fail(f"{installer_path} does not use the canonical repository")
    if "create_draft=true" not in artifact_text:
        fail(f"{artifact_workflow} does not force newly created releases to start as drafts")
    if "matrix.msvc_arch == ''" in artifact_text:
        fail(f"{artifact_workflow} contains an unreachable empty-architecture branch")
    if "msvc_arch: arm64" not in artifact_text:
        fail(f"{artifact_workflow} does not declare the native Windows ARM64 target")
    if artifact_text.count('-DLLVM_DIR="C:/Program Files/LLVM/lib/cmake/llvm"') < 4:
        fail(f"{artifact_workflow} does not provide LLVM_DIR for all Windows targets")
    if "-DZITH_HAS_LLVM=OFF" in artifact_text:
        fail(f"{artifact_workflow} disables LLVM for a release artifact")
    def job_section(text: str, job: str) -> str:
        start = text.find(f"\n  {job}:")
        if start < 0:
            fail(f"{artifact_workflow} is missing the {job} job")
        next_match = re.search(r"\n  [A-Za-z0-9_-]+:\n", text[start + 1 :])
        end = start + 1 + next_match.start() if next_match else len(text)
        return text[start:end]

    for job in ("build-main", "build-musl", "build-lsp"):
        section = job_section(artifact_text, job)
        if "-DZITH_HAS_LLVM=ON" not in section:
            fail(f"{artifact_workflow} does not enable LLVM in {job}")
        if "-DZITH_REQUIRE_LLVM=ON" not in section:
            fail(f"{artifact_workflow} does not require LLVM in {job}")
        if "verify-llvm-build.py" not in section:
            fail(f"{artifact_workflow} does not verify LLVM in {job}")
    for job in ("build-main", "build-musl", "package-stdlib", "build-lsp", "build-wasm"):
        section = job_section(artifact_text, job)
        if "uses: softprops/action-gh-release@v2" in section and "draft: true" not in section:
            fail(f"{artifact_workflow} may publish assets before gates pass in {job}")
    musl_section = job_section(artifact_text, "build-musl")
    if "alpine:3.22" not in musl_section:
        fail(f"{artifact_workflow} does not build musl artifacts in Alpine")
    if "apk add --no-cache" not in musl_section:
        fail(f"{artifact_workflow} does not install Alpine build dependencies")
    for package in ("clang20", "llvm20-dev", "llvm20-static", "lld20"):
        if package not in musl_section:
            fail(f"{artifact_workflow} does not install {package} for musl builds")
    for platform in ("linux/amd64", "linux/arm64"):
        if f"container_platform: {platform}" not in musl_section:
            fail(f"{artifact_workflow} does not build musl for {platform}")
    if "clang-20 --print-target-triple" not in musl_section:
        fail(f"{artifact_workflow} does not detect the Alpine compiler target")
    if '--expected-target "$compiler_target"' not in musl_section:
        fail(f"{artifact_workflow} does not verify the Alpine compiler target")
    if "zig_target" in musl_section or "ZIG_VERSION" in artifact_text:
        fail(f"{artifact_workflow} retains the removed Zig musl toolchain")
    for job in ("build-main", "build-lsp"):
        section = job_section(artifact_text, job)
        if "llvm_asset: win64" not in section or "llvm_asset: woa64" not in section:
            fail(f"{artifact_workflow} does not select Windows LLVM assets per architecture in {job}")
        if "Install Windows ARM64 dependencies" not in section:
            fail(f"{artifact_workflow} does not install native Windows ARM64 LLVM in {job}")
        if "Expose DIA SDK at LLVM's configured path" not in section:
            fail(f"{artifact_workflow} does not expose the DIA SDK to LLVM in {job}")
        if "VSINSTALLDIR" not in section or "diaguids.lib" not in section:
            fail(f"{artifact_workflow} does not locate the active Visual Studio DIA SDK in {job}")
    publish_section = job_section(artifact_text, "publish-release")
    if "if: needs.create-release.outputs.draft == 'false'" not in publish_section:
        fail(f"{artifact_workflow} does not gate final publication on draft mode")
    if "--draft=false" not in publish_section:
        fail(f"{artifact_workflow} does not publish only after the release gates")
    wasm_section = job_section(artifact_text, "build-wasm")
    if "-DZITH_IS_WASM=ON" not in wasm_section:
        fail(f"{artifact_workflow} does not configure the WASM release job")
    if "-DZITH_REQUIRE_LLVM=ON" in wasm_section:
        fail(f"{artifact_workflow} requires native LLVM in the WASM job")

    validation_section = job_section(artifact_text, "validate-release-assets")
    if "actions/checkout@v4" not in validation_section:
        fail(f"{artifact_workflow} does not checkout the release asset validator")
    if "scripts/validate-release-assets.py" not in validation_section:
        fail(f"{artifact_workflow} does not use the release asset validator")
    for asset in RELEASE_ASSETS:
        if asset not in validation_section:
            fail(f"{artifact_workflow} does not validate release asset {asset}")
    distribution_section = job_section(artifact_text, "update-distribution")
    for smoke_job in (
        "smoke-installers-unix",
        "smoke-installer-windows",
        "smoke-installer-wasm",
    ):
        if smoke_job not in distribution_section:
            fail(
                f"{artifact_workflow} updates distribution before {smoke_job} passes"
            )
    if "group: zith-distribution-metadata" not in distribution_section:
        fail(f"{artifact_workflow} does not serialize distribution metadata updates")
    for marker in (
        "scripts/update-scoop-manifest.py",
        "scripts/update-homebrew-formula.py",
        "zithc-stdlib-${{ steps.tag.outputs.tag }}.zip",
        "sha256sum source-archive.tar.gz",
        "--sha256 \"$SHA\"",
        "Sync Homebrew tap formula",
        "base64 --wrap=0 .github/homebrew/zithc.rb",
        "contents/Formula/zithc.rb",
        "gh api --method PUT",
    ):
        if marker not in distribution_section:
            fail(f"{artifact_workflow} is missing distribution step {marker}")

    for job in (
        "smoke-installers-unix:",
        "smoke-installer-windows:",
        "smoke-installer-wasm:",
        "smoke-scoop:",
    ):
        if job not in artifact_text:
            fail(f"{artifact_workflow} is missing the {job[:-1]} release smoke job")
    if "examples/loops-simple.zith" not in artifact_text:
        fail(f"{artifact_workflow} installer smoke uses no C-interop-free Zith program")
    if "test-import-console.zith" in artifact_text:
        fail(f"{artifact_workflow} installer smoke depends on optional libclang interop")
    publish_section = job_section(artifact_text, "publish-release")
    if "if: needs.create-release.outputs.draft == 'false'" not in publish_section:
        fail(f"{artifact_workflow} publishes without honoring draft mode")
    for prerequisite in ("sync-playground-wasm", "smoke-scoop"):
        if prerequisite not in publish_section:
            fail(f"{artifact_workflow} publishes before {prerequisite} passes")
    playground_section = job_section(artifact_text, "sync-playground-wasm")
    for smoke_job in (
        "smoke-installers-unix",
        "smoke-installer-windows",
        "smoke-installer-wasm",
    ):
        if smoke_job not in playground_section:
            fail(
                f"{artifact_workflow} syncs playground before {smoke_job} passes"
            )
    if "build-musl" in ci_text and "-DZITH_HAS_LLVM=OFF" in ci_text:
        fail(f"{ci_workflow} disables LLVM in the musl build")
    if "-DZITH_REQUIRE_LLVM=ON" not in ci_text:
        fail(f"{ci_workflow} does not require LLVM for native builds")
    if "-DLLVM_DIR=#{llvm_dir}" not in formula_text:
        fail("Homebrew formula does not pass the keg-only LLVM CMake directory")
    if "-DZITH_REQUIRE_LLVM=ON" not in formula_text:
        fail("Homebrew formula does not require LLVM")
    for marker in (
        "scripts/update-scoop-manifest.py",
        "scripts/update-homebrew-formula.py",
        "curl --fail --silent --show-error --location --retry 3 --retry-all-errors",
        "sha256sum source-archive.tar.gz",
        "git add .github/scoop/bucket/zithc.json .github/homebrew/zithc.rb",
        "Sync Homebrew tap formula",
        "base64 --wrap=0 .github/homebrew/zithc.rb",
        "contents/Formula/zithc.rb",
        "gh api --method PUT",
    ):
        if marker not in repair_text:
            fail(f"{repair_workflow} is missing shared distribution step {marker}")
    if 'curl -sL "$URL" | sha256sum' in repair_text:
        fail(f"{repair_workflow} hashes an unchecked curl pipeline")
    if "group: zith-distribution-metadata" not in repair_text:
        fail(f"{repair_workflow} does not share the distribution concurrency group")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--workflow-dir", type=Path)
    parser.add_argument("--allow-empty-hashes", action="store_true")
    args = parser.parse_args()
    try:
        validate_manifest(args.manifest, args.allow_empty_hashes)
        if args.workflow_dir is not None:
            validate_workflows(args.workflow_dir)
    except ValueError as error:
        print(f"release contract error: {error}", file=sys.stderr)
        return 1
    print(f"release contract valid: {args.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
