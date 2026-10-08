# Release Stdlib Layout (archived audit)

> Status: archived audit snapshot. It is not an active feature plan. Keep it
> only as history for the release/installer audit; current release behavior
> should be read from `.github/workflows/` and `docs/implementation-debt.md`.

This plan records how the released stdlib archives map to the compiler's
`findStdlibRoots()` roots on each supported install path.

`findStdlibRoots()` checks, in order:

1. `ZITH_STDLIB` when it points to an existing directory.
2. `<binary_dir>/../share/zith/stdlib`.
3. `<binary_dir>/../stdlib`.

The release archives created by `.github/workflows/build-artifact.yml` contain
the contents of `stdlib/` without an enclosing `stdlib/` directory. Tar
and zip assets are therefore extracted directly into the destination root.
Each installer clears the existing destination before extracting, so older
versions do not leave stale stdlib files behind.

## Per-Platform Mapping

| Platform / channel | Release artifact | Binary location | Stdlib destination | `findStdlibRoots()` root |
| --- | --- | --- | --- | --- |
| CMake install | installed from repo `stdlib/` | `<prefix>/bin/zithc` | GNUInstallDirs data root: `<prefix>/share/zith/stdlib` | `<prefix>/share/zith/stdlib` |
| `install.sh`, Unix | `zithc-stdlib-<tag>.tar.gz` | `/usr/local/bin/zithc` | `/usr/local/share/zith/stdlib` | `/usr/local/share/zith/stdlib` |
| `install.sh`, MSYS/MinGW/Cygwin | `zithc-stdlib-<tag>.zip` | `~/.local/bin/zithc.exe` | `~/.local/share/zith/stdlib` | `~/.local/share/zith/stdlib` |
| `install.ps1` | `zithc-stdlib-<tag>.zip` | `%LOCALAPPDATA%\Zith\bin\zithc.exe` | `%LOCALAPPDATA%\Zith\share\zith\stdlib` | `%LOCALAPPDATA%\Zith\share\zith\stdlib` |
| Scoop manifest | `zithc-stdlib-<tag>.zip` | `<scoop apps>\zithc\<version>\zithc.exe` | `<scoop apps>\zithc\share\zith\stdlib` | `<scoop apps>\zithc\share\zith\stdlib` when the real exe is invoked directly |

Scoop installs a shim on `PATH`, so a user running `zithc` through the shim may
not exercise the exe-relative discovery path. Direct invocation of the actual
exe works; otherwise `ZITH_STDLIB` or `--include` is the reliable workaround.

## Smoke Commands

After a shell script install on Unix:

```bash
zithc --include /usr/local/share/zith/stdlib check examples/hello-world.zith
```

After an MSYS/MinGW/Cygwin install:

```bash
"$HOME/.local/bin/zithc" --include "$HOME/.local/share/zith/stdlib" check examples/hello-world.zith
```

After a PowerShell install:

```powershell
zithc --include "$env:LOCALAPPDATA\Zith\share\zith\stdlib" check examples\hello-world.zith
```

These commands avoid relying on `ZITH_STDLIB` and confirm the installed stdlib
is used from the channel-specific root.
