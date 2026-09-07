# Release Install Layout Memory

This note records the release artifact contract and the fixes made to
`scripts/install.sh` so the installer puts the stdlib where
`findStdlibRoots()` discovers it. It is the durable reference for the
package/installer audit that touched this file.

## Current Behavior

`build-artifact.yml` uploads a standalone `zithc` binary per platform plus
two stdlib archives built from the repo `stdlib/` tree:

| Platform family | Binary asset | Bundle format |
|---|---|---|
| Linux glibc amd64 | `zithc-linux-amd64` | `zithc-stdlib-<tag>.tar.gz` |
| Linux glibc arm64 | `zithc-linux-arm64` | `zithc-stdlib-<tag>.tar.gz` |
| Linux musl amd64 | `zithc-linux-amd64-musl` | `zithc-stdlib-<tag>.tar.gz` |
| Linux musl arm64 | `zithc-linux-arm64-musl` | `zithc-stdlib-<tag>.tar.gz` |
| macOS universal | `zithc-macos-universal` | `zithc-stdlib-<tag>.tar.gz` |
| Windows amd64 | `zithc-windows-amd64.exe` | `zithc-stdlib-<tag>.zip` |

Release tags are `vX.Y.Z`, and both `install.sh` and `install.ps1` build
download URLs from the release download endpoint under the repo owner and
version tag. The stdlib archives have no top-level directory, so extraction
into a fresh `stdlib/` directory yields `c/`, `soon/`, and `std/` directly
under it.

## Discovery Contract

`src/support/stdlib-discovery.cpp` exposes `findStdlibRoots()`. After the
`ZITH_STDLIB` override, it checks these paths relative to the running binary:

| Path | Meaning |
|---|---|
| `<binary_dir>/../share/zith/stdlib` | installed release layout |
| `<binary_dir>/../stdlib` | dev/build layout |

`CMakeLists.txt` installs the stdlib to `${CMAKE_INSTALL_LIBDIR}/zith/stdlib`,
which is the source-install layout. The release installer uses the discovery
root because published binaries are downloaded directly rather than installed
by CMake.

For a conventional Unix prefix, `CMAKE_INSTALL_LIBDIR` defaults to `lib`, so
`/usr/local/lib/zith/stdlib` does not match the release installer's discovery
root (`/usr/local/share/zith/stdlib`). The installed binary lives in
`/usr/local/bin`, so `<binary_dir>/../share/zith/stdlib` resolves to the
installer path. The source install rule and the release installer intentionally
use different roots; `findStdlibRoots()` does not search
`${CMAKE_INSTALL_LIBDIR}/zith/stdlib`.

## Installer Fixes

`scripts/install.sh` now treats a user-provided version without a leading `v`
as `v<version>` before using it in GitHub release URLs. The previous script
only handled a raw tag, which made bare versions fail for both the binary and
stdlib assets.

The Unix path installs the stdlib under
`/usr/local/share/zith/stdlib`. Before extraction it removes the existing
directory and recreates it, so stale files from an older stdlib bundle do not
survive an upgrade.

Running on MINGW/MSYS/CYGWIN now installs the binary to
`$ZITH_PREFIX/bin` (default `~/.local/bin`) and extracts the stdlib zip to
`$ZITH_PREFIX/share/zith/stdlib`. This is the same `share/zith/stdlib` pattern
the compiler discovers from a binary in `<prefix>/bin`, instead of leaving the
stdlib next to a binary in the current directory where discovery cannot find
it after the user moves the binary.

No `--strip-components` flag is used for the tar archive because the stdlib
bundle has no top-level directory. `rm -rf "$STDLIB_DIR"` before extraction is
the stale-file protection.

## Verification Smoke

After installing a release, verify discovery without an environment override:

```bash
unset ZITH_STDLIB
/usr/local/bin/zithc --include "" check /tmp/hello.zith
```

The same check applies to a `$ZITH_PREFIX` installation by using
`$ZITH_PREFIX/bin/zithc` and `$ZITH_PREFIX/share/zith/stdlib`.

## Out Of Scope

This task does not change `build-artifact.yml`, `install.ps1`, Scoop, Homebrew,
or `stdlib-discovery.cpp` semantics. Those are covered by other packaging audit
tasks and remain separate coordination surfaces until their branches land.
