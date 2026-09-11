# Release Artifact Matrix (archived audit)

> Status: archived audit snapshot. It is not an active feature plan. Keep it
> only as history for the release/installer audit; current release behavior
> should be read from `.github/workflows/` and `docs/implementation-debt.md`.

This document records the intended release artifact contract and the flags
each target should use in the build-artifact workflow. Native `zithc` x86-64
binaries are the compiler distribution and must ship with LLVM codegen so the
release path matches the "LLVM 18+ for the codegen backend" contract. ARM64
and the macOS universal slice are blockers until the codegen backend and CMake
linkage cover AArch64; the standalone native toolchain ADRs are still proposed
and do not currently remove codegen from released binaries.

## Native `zithc` Matrix

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` | `zithc-linux-amd64` | ON (fixed) | removed (legacy) | OFF | OFF | `zithc` |
| `ubuntu-latest` | `zithc-linux-arm64` | OFF (blocker) | removed (legacy) | OFF | OFF | `zithc` |
| `macos-14` | `zithc-macos-universal` | OFF (blocker) | removed (legacy) | OFF | OFF | `zithc` |
| `windows-latest` | `zithc-windows-amd64.exe` | ON (fixed) | removed (legacy) | OFF | OFF | `zithc.exe` |
| `windows-latest` | `zithc-windows-arm64.exe` | OFF (blocker) | removed (legacy) | OFF | OFF | `zithc.exe` |

## Musl `zithc` Matrix

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` | `zithc-linux-amd64-musl` | OFF (blocker) | removed (legacy) | OFF | OFF | `zithc` |
| `ubuntu-latest` | `zithc-linux-arm64-musl` | OFF (blocker) | removed (legacy) | OFF | OFF | `zithc` |

Musl stays OFF because the static Zig build cannot link the host LLVM 18 stack
used by the native x86-64 targets. Before shipping native codegen in musl
binaries, the standalone bundle work in `docs/adr/0006` must provide a
musl-compatible LLVM/LLD runtime or the musl artifacts must be labeled
check-only/sema-only.

## LSP and Wasm Matrices

| Runner | target_name | `ZITH_HAS_LLVM` | `ZITH_ENABLE_FFI` | `ZITH_ENABLE_C_COMPILE` | `ZITH_IS_WASM` | Artifact |
| --- | --- | --- | --- | --- | --- | --- |
| `ubuntu-latest` | `zith-lsp-linux-amd64` | ON | removed (legacy) | OFF | OFF | `zith-lsp` |
| `ubuntu-latest` | `zith-lsp-linux-arm64` | OFF (blocker) | removed (legacy) | OFF | OFF | `zith-lsp` |
| `macos-14` | `zith-lsp-macos-universal` | OFF (blocker) | removed (legacy) | OFF | OFF | `zith-lsp` |
| `windows-latest` | `zith-lsp-windows-amd64.exe` | ON | removed (legacy) | OFF | OFF | `zith-lsp.exe` |
| `windows-latest` | `zith-lsp-windows-arm64.exe` | OFF (blocker) | removed (legacy) | OFF | OFF | `zith-lsp.exe` |
| `ubuntu-latest` | `zithc-wasm.zip` | OFF (no LLVM codegen) | removed (legacy) | OFF | ON | wasm bundle |

`zith-lsp` can operate without LLVM, and the extension explicitly handles a
`codegenAvailable: false` warning. Keep ARM64 and the macOS universal LSP
target OFF because the codegen backend does not initialize or link AArch64 yet;
bring them back when the target libraries and `CodeGen` initialization cover
AArch64.

## Installer/Release Smoke Path

- `scripts/install.sh` matches `zithc-linux-amd64`, `zithc-linux-arm64`,
  `zithc-macos-universal`, and `zithc-windows-amd64.exe`.
- `scripts/install.sh --musl` matches `zithc-linux-*-musl`.
- `scripts/install.ps1` matches `zithc-windows-amd64.exe` and
  `zithc-windows-arm64.exe`, plus `zithc-stdlib-<tag>.zip`.
- Scoop uses `zithc-windows-amd64.exe`, `zithc-windows-arm64.exe`, and
  `zithc-stdlib-<version>.zip` in `.github/scoop/bucket/zithc.json`.
- Homebrew dispatches a source-archive `sha256` through
  `.github/workflows/update-package.yml`; the reference formula records the
  `ZITH_HAS_LLVM=OFF -DZITH_ENABLE_FFI=OFF` contract that must be updated when
  the tag is bumped.

## Open Blockers

- `CodeGen` initializes x86 and WebAssembly targets only
  (`src/codegen/codegen.cpp` and `src/codegen/codegen-type.cpp`), and `emit()`
  explicitly rejects state machines on non-x86/Wasm triples. ARM64 release
  binaries must stay OFF, and the macOS universal compiler cannot claim ARM64
  codegen, until AArch64 TargetInfo/Target/TargetMC/AsmParser/AsmPrinter plus
  the CMake `AArch64CodeGen`/`AArch64AsmParser`/`AArch64Desc`/`AArch64Info`
  components are added and tested.
- `scripts/install.ps1 --musl` requests `zithc-windows-amd64-musl.exe` and
  `zithc-windows-arm64-musl.exe`, but `build-artifact.yml` has no windows-musl
  targets. Installer/package manifest edits are out of scope for this audit;
  either add the targets or remove the Windows `--musl` branch.
- The Homebrew reference formula still disables LLVM and references the legacy
  `ZITH_ENABLE_FFI` option. It is outside the workflow file but should follow
  this matrix: the root `CMakeLists.txt` no longer defines `ZITH_ENABLE_FFI`.
- The standalone bundle ADRs are proposed, not accepted. Until the LLVM/LLD
  bundle exists, native codegen binaries rely on LLVM 18+ installed at release
  build time and CMake links the detected LLVM libraries into `zithc`.
