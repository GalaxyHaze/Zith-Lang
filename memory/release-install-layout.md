# Release Install Layout Memory

Short operational pointer for the release/installer layout. The audit snapshot
with the per-platform artifact matrix and stdlib destinations is archived at
`docs/plans/archive/release-artifacts.old.md` and
`docs/plans/archive/release-stdlib.old.md`. Current release behavior lives in
`.github/workflows/`. This note keeps only the discovery contract and the
installer fixes that are easy to regress.

## Discovery Contract

`src/support/stdlib-discovery.cpp` exposes `findStdlibRoots()`, which checks in
order:

1. `ZITH_STDLIB` when it points to an existing directory.
2. `<binary_dir>/../share/zith/stdlib`.
3. `<binary_dir>/../stdlib`.

`CMakeLists.txt` installs the stdlib to
`${CMAKE_INSTALL_DATADIR}/zith/stdlib`, which for a conventional prefix is
`share/zith/stdlib`, the same root the release installer uses.

## Installer Fixes

- `scripts/install.sh` treats a version without a leading `v` as `v<version>`
  before building GitHub release URLs.
- The Unix path installs the stdlib under `/usr/local/share/zith/stdlib` and
  removes the existing directory before extraction, so stale files from an
  older stdlib bundle do not survive an upgrade.
- MSYS/MinGW/Cygwin installs the binary to `$ZITH_PREFIX/bin` and extracts the
  stdlib zip to `$ZITH_PREFIX/share/zith/stdlib`.
- The stdlib tar archive has no top-level directory, so no `--strip-components`
  flag is used. `rm -rf "$STDLIB_DIR"` before extraction is the stale-file
  protection.

## Smoke Check

```bash
zithc --include /usr/local/share/zith/stdlib check examples/hello-world.zith
```
