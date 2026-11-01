#!/usr/bin/env python3
"""Unit-test the macOS runtime bundler without Mach-O tools."""

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("package-macos-runtime.py")
SPEC = importlib.util.spec_from_file_location("package_macos_runtime", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
RUNTIME = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNTIME)


class RuntimePackageTests(unittest.TestCase):
    def test_parse_otool_dependencies_and_rpaths(self) -> None:
        dependencies = RUNTIME.parse_dependencies(
            "binary:\n"
            "\t@rpath/libLLVM.dylib (compatibility version 1.0.0, current version 1.0.0)\n"
            "\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1.0.0)\n"
        )
        rpaths = RUNTIME.parse_rpaths(
            "Load command 1\n"
            "      cmd LC_RPATH\n"
            "  cmdsize 48\n"
            "     path /opt/homebrew/opt/llvm/lib (offset 12)\n"
            "Load command 2\n"
        )
        self.assertEqual(dependencies, ("@rpath/libLLVM.dylib", "/usr/lib/libSystem.B.dylib"))
        self.assertEqual(rpaths, ("/opt/homebrew/opt/llvm/lib",))

    def test_bundle_walks_transitive_dependencies_and_rewrites_paths(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zith-macos-runtime-") as raw:
            root = Path(raw)
            compiler = root / "bin/zithc"
            language_server = root / "bin/zith-lsp"
            llvm = root / "homebrew/lib/libLLVM.dylib"
            clang = root / "homebrew/lib/libclang.dylib"
            zstd = root / "homebrew/lib/libzstd.1.dylib"
            for path, content in (
                (compiler, b"macho compiler"),
                (language_server, b"macho language server"),
                (llvm, b"llvm library"),
                (clang, b"clang library"),
                (zstd, b"zstd library"),
            ):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)

            dependency_output = {
                str(compiler): (
                    f"{compiler}:\n"
                    f"\t{llvm} (compatibility version 1.0.0, current version 1.0.0)\n"
                    "\t/usr/lib/libSystem.B.dylib "
                    "(compatibility version 1.0.0, current version 1.0.0)\n"
                ),
                str(language_server): (
                    f"{language_server}:\n"
                    f"\t{llvm} (compatibility version 1.0.0, current version 1.0.0)\n"
                ),
                str(llvm): (
                    f"{llvm}:\n"
                    "\t@rpath/libclang.dylib "
                    "(compatibility version 1.0.0, current version 1.0.0)\n"
                ),
                str(clang): (
                    f"{clang}:\n"
                    "\t@loader_path/libzstd.1.dylib "
                    "(compatibility version 1.0.0, current version 1.0.0)\n"
                ),
                str(zstd): f"{zstd}:\n",
            }
            rpath_output = {
                str(compiler): "",
                str(language_server): "",
                str(llvm): (
                    "Load command 1\n"
                    "      cmd LC_RPATH\n"
                    "  cmdsize 48\n"
                    f"     path {llvm.parent} (offset 12)\n"
                ),
                str(clang): "",
                str(zstd): "",
            }
            calls: list[tuple[str, ...]] = []

            def fake_runner(*args: str) -> str:
                calls.append(args)
                if args[0] == "otool" and args[1] == "-L":
                    return dependency_output[args[2]]
                if args[0] == "otool" and args[1] == "-l":
                    return rpath_output[args[2]]
                return ""

            runtime_dir = root / "runtime"
            RUNTIME.bundle_binaries(
                (compiler, language_server), runtime_dir, runner=fake_runner
            )

            self.assertEqual(
                sorted(path.name for path in (runtime_dir / "lib").iterdir()),
                ["libLLVM.dylib", "libclang.dylib", "libzstd.1.dylib"],
            )
            manifest = json.loads((runtime_dir / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(set(manifest["libraries"]), {
                "libLLVM.dylib",
                "libclang.dylib",
                "libzstd.1.dylib",
            })
            self.assertIn(
                (
                    "install_name_tool",
                    "-change",
                    str(llvm),
                    "@executable_path/../share/zith/lib/libLLVM.dylib",
                    str(compiler),
                ),
                calls,
            )
            self.assertEqual(
                sum(
                    call[:3] == ("install_name_tool", "-id", "@loader_path/libLLVM.dylib")
                    for call in calls
                ),
                1,
            )
            self.assertIn(
                (
                    "install_name_tool",
                    "-change",
                    "@rpath/libclang.dylib",
                    "@loader_path/libclang.dylib",
                    str(runtime_dir / "lib/libLLVM.dylib"),
                ),
                calls,
            )

    def test_unresolved_non_system_dependency_fails(self) -> None:
        with self.assertRaisesRegex(RUNTIME.BundleError, "cannot resolve @rpath/libLLVM.dylib"):
            RUNTIME.resolve_dependency("@rpath/libLLVM.dylib", Path("/tmp/zithc"), Path("/tmp/zithc"), ())


if __name__ == "__main__":
    unittest.main()
