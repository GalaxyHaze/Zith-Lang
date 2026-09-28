# Release Artifact Matrix (archived audit)

> Status: archived audit snapshot. It is not an active feature plan. Keep it
> only as history for the release/installer audit; current release behavior
> should be read from `.github/workflows/` and `docs/implementation-debt.md`.

This document records the intended release artifact contract and the flags
each target should use in the build-artifact workflow. Every native release
target requests LLVM and fails configuration if LLVM 18+ cannot be found.
ARM64 targets run on native ARM runners, and macOS ships separate arm64 and
amd64 assets instead of a cross-linked universal binary.

## Native `zithc` Matrix

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` | `zithc-linux-amd64` | ON (fixed) | removed (legacy) | OFF | OFF | `zithc` |
| `ubuntu-24.04-arm` | `zithc-linux-arm64` | ON (required) | removed (legacy) | OFF | OFF | `zithc` |
| `macos-14` | `zithc-macos-arm64` | ON (required) | removed (legacy) | OFF | OFF | `zithc` |
| `macos-15-intel` | `zithc-macos-amd64` | ON (required) | removed (legacy) | OFF | OFF | `zithc` |
| `windows-latest` | `zithc-windows-amd64.exe` | ON (fixed) | removed (legacy) | OFF | OFF | `zithc.exe` |
| `windows-11-arm` | `zithc-windows-arm64.exe` | ON (required) | removed (legacy) | OFF | OFF | `zithc.exe` |

Windows release jobs use only the LLVM 20.1.8 MSVC development archive for x64
or ARM64. It provides the compiler, CMake package, and libraries needed to
build Zith with the current MSVC STL. The workflow clears the install directory
before extraction to prevent stale files from another LLVM version from mixing
with the archive.

## Musl `zithc` Matrix

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` with Alpine 3.22 (`linux/amd64`) | `zithc-linux-amd64-musl` | ON (required) | removed (legacy) | OFF | OFF | `zithc` |
| `ubuntu-24.04-arm` with Alpine 3.22 (`linux/arm64`) | `zithc-linux-arm64-musl` | ON (required) | removed (legacy) | OFF | OFF | `zithc` |

The musl jobs build inside native Alpine containers and install Alpine's
target-matched Clang 20 and static LLVM 20 packages, including the separate
`llvm20-gtest` archive package required by LLVM's exported CMake targets. They
verify the compiler's reported musl target and require LLVM, so they fail
rather than publish a sema-only compiler. A successful release run is still
required to confirm both Alpine builds end to end.

## LSP and Wasm Matrices

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` | `zith-lsp-linux-amd64` | ON | removed (legacy) | OFF | OFF | `zith-lsp` |
| `ubuntu-24.04-arm` | `zith-lsp-linux-arm64` | ON (required) | removed (legacy) | OFF | OFF | `zith-lsp` |
| `macos-14` | `zith-lsp-macos-arm64` | ON (required) | removed (legacy) | OFF | OFF | `zith-lsp` |
| `macos-15-intel` | `zith-lsp-macos-amd64` | ON (required) | removed (legacy) | OFF | OFF | `zith-lsp` |
| `windows-latest` | `zith-lsp-windows-amd64.exe` | ON | removed (legacy) | OFF | OFF | `zith-lsp.exe` |
| `windows-11-arm` | `zith-lsp-windows-arm64.exe` | ON (required) | removed (legacy) | OFF | OFF | `zith-lsp.exe` |
| `ubuntu-latest` | `zithc-wasm.zip` | OFF (no LLVM codegen) | removed (legacy) | OFF | ON | wasm bundle |

`zith-lsp` uses the same LLVM-enabled source contract as `zithc`, so diagnostics
and codegen capability do not diverge between release targets.

## Installer/Release Smoke Path

- `scripts/install.sh` matches `zithc-linux-amd64`, `zithc-linux-arm64`,
  `zithc-macos-amd64`, `zithc-macos-arm64`, and `zithc-windows-amd64.exe`.
- `scripts/install.sh --musl` matches `zithc-linux-*-musl`.
- `scripts/install.ps1` matches `zithc-windows-amd64.exe` and
  `zithc-windows-arm64.exe`, plus `zithc-stdlib-<tag>.zip`.
- Scoop uses `zithc-windows-amd64.exe`, `zithc-windows-arm64.exe`, and
  `zithc-stdlib-<version>.zip` in `.github/scoop/bucket/zithc.json`.
- Homebrew receives the complete source formula through the Contents API in the
  `update-distribution` job. The update uses the remote file SHA as a
  compare-and-swap guard, and the reference formula requires LLVM.

## Open Blockers

- `CodeGen` now initializes the AArch64 target in addition to x86 and
  WebAssembly. The native ARM release jobs still need one real execution to
  prove that the hosted LLVM installation and target machine link correctly.
- The Windows runners and the Alpine musl jobs still need one successful
  release execution to confirm the LLVM development archives, DIA SDK path,
  and target-matched static LLVM libraries end to end. Configuration now fails
  instead of silently disabling LLVM.
- Release smoke jobs now exercise the Unix and Windows installers and the Scoop
  manifest. The external tap still needs the first authenticated formula sync
  and a real Homebrew build in `GalaxyHaze/homebrew-zithc`.
- Releases created through `create-new-release.yml` remain draft until the
  final `publish-release` job observes successful asset, installer, distribution
  and playground gates.
- `validate-release-assets` now gates the distribution update on the complete
  native, musl, LSP, WASM and stdlib asset set, preventing package metadata or
  the playground sync from being updated for a partial release.
- The standalone bundle ADRs are proposed, not accepted. Until the LLVM/LLD
  bundle exists, native codegen binaries rely on LLVM 18+ installed at release
  build time and CMake links the detected LLVM libraries into `zithc`.
