#!/usr/bin/env python3
"""Bundle non-system Mach-O dependencies for relocatable macOS release binaries."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable


class BundleError(RuntimeError):
    pass


def run_tool(*args: str) -> str:
    try:
        result = subprocess.run(args, check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        detail = getattr(error, "stderr", None) or str(error)
        raise BundleError(f"command failed: {' '.join(args)}\n{detail}") from error
    return result.stdout


def parse_dependencies(output: str) -> tuple[str, ...]:
    dependencies = []
    for line in output.splitlines():
        match = re.match(r"^\s*(.+?)\s+\(compatibility version [^)]+\)", line)
        if match:
            dependencies.append(match.group(1))
    return tuple(dict.fromkeys(dependencies))


def parse_rpaths(output: str) -> tuple[str, ...]:
    lines = output.splitlines()
    rpaths = []
    for index, line in enumerate(lines):
        if line.strip() != "cmd LC_RPATH":
            continue
        for candidate in lines[index + 1 :]:
            if candidate.strip().startswith("cmd "):
                break
            match = re.match(r"^\s*path (.+?) \(offset [0-9]+\)$", candidate)
            if match:
                rpaths.append(match.group(1))
                break
    return tuple(dict.fromkeys(rpaths))


def is_system_path(path: Path) -> bool:
    value = str(path)
    return value.startswith(("/usr/lib/", "/System/Library/", "/Library/Apple/System/"))


def expand_loader_path(value: str, loader: Path, executable: Path) -> Path:
    if value == "@loader_path":
        return loader.parent
    if value.startswith("@loader_path/"):
        return loader.parent / value.removeprefix("@loader_path/")
    if value == "@executable_path":
        return executable.parent
    if value.startswith("@executable_path/"):
        return executable.parent / value.removeprefix("@executable_path/")
    return Path(value)


def resolve_dependency(
    dependency: str,
    loader: Path,
    executable: Path,
    rpaths: tuple[str, ...],
) -> Path | None:
    if dependency.startswith("/"):
        candidates = [Path(dependency)]
    elif dependency.startswith("@loader_path/") or dependency == "@loader_path":
        candidates = [expand_loader_path(dependency, loader, executable)]
    elif dependency.startswith("@executable_path/") or dependency == "@executable_path":
        candidates = [expand_loader_path(dependency, loader, executable)]
    elif dependency.startswith("@rpath/"):
        suffix = dependency.removeprefix("@rpath/")
        candidates = [
            expand_loader_path(rpath, loader, executable) / suffix for rpath in rpaths
        ]
    else:
        raise BundleError(f"unsupported Mach-O dependency name: {dependency}")

    for candidate in candidates:
        if is_system_path(candidate):
            return None
        if candidate.is_file():
            resolved = candidate.resolve()
            if is_system_path(resolved):
                return None
            return resolved
    if any(is_system_path(candidate) for candidate in candidates):
        return None
    search_list = ", ".join(str(candidate) for candidate in candidates) or "<no LC_RPATH>"
    raise BundleError(f"cannot resolve {dependency} from {loader}; searched {search_list}")


def _library_name(source: Path, used_names: dict[str, tuple[str, str]]) -> str:
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    name = source.name
    existing = used_names.get(name)
    if existing is None or existing[1] == digest:
        used_names[name] = (str(source), digest)
        return name

    path = Path(name)
    unique_name = f"{path.stem}-{digest[:10]}{path.suffix}"
    existing = used_names.get(unique_name)
    if existing is not None and existing[1] != digest:
        raise BundleError(f"cannot assign a unique runtime name to {source}")
    used_names[unique_name] = (str(source), digest)
    return unique_name


def bundle_binaries(
    binaries: tuple[Path, ...],
    runtime_dir: Path,
    runner: Callable[..., str] = run_tool,
    executable_relative_lib_dir: str = "../share/zith/lib",
) -> None:
    if not binaries:
        raise BundleError("at least one --binary is required")

    binary_paths = tuple(path.resolve(strict=True) for path in binaries)
    library_dir = runtime_dir / "lib"
    library_dir.mkdir(parents=True, exist_ok=True)
    source_to_name: dict[Path, str] = {}
    used_names: dict[str, tuple[str, str]] = {}
    inspected: dict[tuple[Path, Path], tuple[tuple[str, ...], tuple[str, ...]]] = {}
    rewrites: dict[tuple[Path, Path], dict[str, str]] = {}
    pending = [(binary, binary) for binary in binary_paths]
    visited: set[tuple[Path, Path]] = set()

    while pending:
        loader, executable = pending.pop()
        key = (loader, executable)
        if key in visited:
            continue
        visited.add(key)

        dependencies = parse_dependencies(runner("otool", "-L", str(loader)))
        rpaths = parse_rpaths(runner("otool", "-l", str(loader)))
        inspected[key] = (dependencies, rpaths)
        object_rewrites = rewrites.setdefault(key, {})

        for dependency in dependencies:
            source = resolve_dependency(dependency, loader, executable, rpaths)
            if source is None:
                continue
            name = source_to_name.get(source)
            if name is None:
                name = _library_name(source, used_names)
                source_to_name[source] = name
                pending.append((source, executable))
            previous = object_rewrites.setdefault(dependency, name)
            if previous != name:
                raise BundleError(
                    f"dependency {dependency} resolves inconsistently for {loader}"
                )

    for source, name in source_to_name.items():
        destination = library_dir / name
        if destination.exists():
            if hashlib.sha256(destination.read_bytes()).hexdigest() != hashlib.sha256(
                source.read_bytes()
            ).hexdigest():
                raise BundleError(f"runtime destination already contains different data: {destination}")
        else:
            shutil.copy2(source, destination)

    library_outputs = {
        library_dir / name for name in source_to_name.values()
    }
    for output in sorted(library_outputs):
        runner("install_name_tool", "-id", f"@loader_path/{output.name}", str(output))

    output_rewrites: dict[Path, dict[str, str]] = {}
    output_rpaths: dict[Path, set[str]] = {}
    for key, object_rewrites in rewrites.items():
        loader, executable = key
        is_library = loader in source_to_name
        output = library_dir / source_to_name[loader] if is_library else loader
        output_rpaths.setdefault(output, set()).update(inspected[key][1])
        merged_rewrites = output_rewrites.setdefault(output, {})
        for old_name, name in object_rewrites.items():
            if is_library:
                new_name = f"@loader_path/{name}"
            else:
                new_name = f"@executable_path/{executable_relative_lib_dir}/{name}"
            previous = merged_rewrites.setdefault(old_name, new_name)
            if previous != new_name:
                raise BundleError(
                    f"dependency {old_name} resolves inconsistently for {output}"
                )

    for output, object_rewrites in output_rewrites.items():
        for old_name, new_name in object_rewrites.items():
            if old_name != new_name:
                runner("install_name_tool", "-change", old_name, new_name, str(output))
        for rpath in sorted(output_rpaths[output]):
            runner("install_name_tool", "-delete_rpath", rpath, str(output))

    manifest = {
        name: hashlib.sha256((library_dir / name).read_bytes()).hexdigest()
        for name in sorted(source_to_name.values())
    }
    (runtime_dir / "manifest.json").write_text(
        json.dumps({"libraries": manifest}, indent=2) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, action="append", required=True)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    args = parser.parse_args()

    if sys.platform != "darwin":
        parser.error("Mach-O runtime packaging must run on macOS")
    try:
        required_tools = ("otool", "install_name_tool")
        missing = [tool for tool in required_tools if shutil.which(tool) is None]
        if missing:
            raise BundleError(f"missing macOS tools: {', '.join(missing)}")
        bundle_binaries(tuple(args.binary), args.runtime_dir)
    except (BundleError, OSError) as error:
        print(f"macOS runtime packaging failed: {error}", file=sys.stderr)
        return 1

    print(f"macOS runtime packaged at {args.runtime_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
