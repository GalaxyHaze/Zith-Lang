# Package Lockfile

The package lockfile contract lives in ADR 0048 and the package-manager plan.
Keep lockfile paths project-relative for checkout portability. Validate raw
TOML path strings before constructing `std::filesystem::path`, which can
normalize repeated separators, and compare resolved roots with a key that
removes trailing separators because `lexically_normal()` can preserve them.
Give raw-string and `path` validators distinct names because `std::string`
arguments can make overload resolution ambiguous. `ZITH_LIB_SOURCES` uses
`file(GLOB_RECURSE)` without `CONFIGURE_DEPENDS`, so rerun CMake configuration
after adding a new source file.

SHA-256 appends the message bit length in big-endian order. The empty-input
vector cannot detect a reversed length field, so keep non-empty and multi-block
known vectors in the package tests.

## Lockfile gate and project scaffolding

`zithc check`, `zithc build`, and `zithc run` all require a valid
`Zith.lock` when the project has package metadata (`[project]` in
`ZithProject.toml`). The gate lives in `src/cli/options.cpp` and reports
`run 'zithc deps resolve'` when the lockfile is missing or stale. `run` was
initially left out of the gate even though it performs the same native build;
it is now included. `zithc execute` intentionally does not gate, because it
runs an already-built binary and never reads the package graph.

`zithc create <name>` resolves the empty dependency graph and writes
`Zith.lock` right after scaffolding, so a fresh project passes the gate without
an extra `deps resolve` step. Regression coverage lives in
`tests/test-cli-commands.cpp` (`test_create_writes_lockfile_for_generated_project`
and `test_check_build_require_explicit_lockfile`).
