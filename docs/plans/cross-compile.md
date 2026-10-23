# Bootstrap Compiler Cross-Compile Strategy

Status: strategy. The compiler architecture and target matrix are not yet
implemented in this repository. This plan concerns building the bootstrap
compiler; it does not define the final source-language or C-migration state.
Sequence target expansion after the host bootstrap is reproducible, as
described in the [project roadmap](roadmap.md).

## Goal

Produce Zith compiler binaries for several targets, preferably locally on the
development host (Void Linux x86-64), without depending on CI runners for
anything that can be done locally.

## Two Axes

Two different things, easy to conflate:

- **Build the compiler** for target T: produce a Zith compiler binary that runs
  on T.
- **The compiler produces programs** for target T: the compiler running on the
  host emits code and objects for T.

Both reduce to the same missing pieces per target: a triple, a sysroot (headers
and libc), a linker, and compiler-rt. The initial compiler core is
target-agnostic ([ADR-0012](../adr/0012-os-sensitive-code-is-a-c-layer.md));
portability lives in the OS-C layer and the C toolchain adapter
([ADR-0013](../adr/0013-c-toolchain-is-an-adapter.md)).

## Portability Boundary

The initial design places OS-sensitive calls behind a C-layer interface. This
is a bootstrap boundary to review as project-owned C code migrates to Zith;
target support must remain isolated from frontend, sema, and IR regardless of
the eventual implementation language. The C toolchain is a
`ToolchainBackend` adapter (native compile, link, header import, sysroot
discovery), chosen in a deterministic order: clang + `--target` + `lld` first,
Zig second, a native cross-GCC third.

## Target Matrix

| Target | Triple | Toolchain | Local on Void? | Test |
|---|---|---|---|---|
| Linux amd64 | `x86_64-linux-gnu` | system clang + `lld` | yes | native |
| Linux arm64 | `aarch64-linux-gnu` | `cross-aarch64-linux-gnu`, or clang + sysroot | yes | qemu-user |
| Linux amd64 musl | `x86_64-linux-musl` | `cross-*` musl, or Zig | yes | native |
| Linux arm64 musl | `aarch64-linux-musl` | `cross-aarch64-linux-musl`, or Zig | yes | qemu-user |
| WASM/WASI | `wasm32-wasi` | `wasi-sdk`, or Zig | yes | wasmtime / node |
| Windows GNU | `x86_64-windows-gnu` | Zig, or `cross-x86_64-w64-mingw32` | yes | wine |
| Windows MSVC | `x86_64-pc-windows-msvc` | `xwin` + clang-cl + `lld-link` | yes (build) | wine (partial) |
| macOS arm64/amd64 | `arm64-apple-macos` / `x86_64-apple-macos` | Apple toolchain + SDK on a Mac; osxcross with a supplied SDK | conditional (local SDK) | on a Mac |

## Phases

1. **Host only.** Linux amd64 with the system clang + `lld`. No new installs.
2. **Linux cross.** Add arm64 and musl via Void `cross-*` packages or Zig.
   Covers most Linux distribution.
3. **Windows GNU.** Add the Zig adapter (covers Win-GNU), or the Void mingw
   cross packages.
4. **Windows MSVC.** Add the `xwin` + clang-cl + `lld-link` path if the MSVC ABI
   is required.
5. **WASM.** Add `wasi-sdk` or the Zig wasm targets when the playground driver
   needs it.
6. **macOS.** Defer build and distribution commitments until the host compiler
   and SDK policy have been reviewed (see
   [macOS build policy](../notes/macos-build-policy.md)).

## Notes

- Zig is the fallback adapter for any target whose sysroot is not worth
  maintaining by hand: it bundles libc, sysroot, linker, and compiler-rt. Its
  version matters (the sibling Zith CI pins `0.13.0`).
- `xwin` is not in the Void repositories; obtain it separately. It fetches the
  Microsoft CRT and Windows SDK headers and libraries under Microsoft's own
  terms.
- clang is already a cross-compiler; the missing piece per target is always the
  sysroot and linker, never the compiler front end.
- The sibling Zith release workflow is the reference for what each target needs:
  native runners for macOS and Windows-MSVC, Docker-alpine (Zig/clang) for musl.
  The difference here is that Windows-MSVC can also be built locally with
  `xwin`.
