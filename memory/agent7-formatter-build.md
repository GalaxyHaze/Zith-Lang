# Agent7 Formatter Verification and Build Notes

Summary: the `for (cond)` / `for { }` formatter round-trip is represented by
two frontend provenance flags on the `While`-kind AST node
(`Expression::isForSpelling` and `Expression::forNoCondition`). The flags keep
the existing sema/HIR path unchanged while allowing `FmtVisitor` to emit the
source spelling that was used.

## Accepted behavior

- `for (cond) { }` parses to `ExprKind::While` with `isForSpelling = true`,
  so `FmtVisitor` emits `for (cond) { ... }`.
- `for { }` also parses to `ExprKind::While` and has `forNoCondition = true`,
  so `FmtVisitor` suppresses the synthetic `true` condition and emits
  `for { ... }`.
- `while (cond)` keeps `isForSpelling = false` and still emits `while (...)`.
  `parseWhile()` continues to report `W1008`.
- 3-clause `for` and `for (name in iterable)` keep their existing
  `ExprKind::For` / `ExprKind::ForIn` representation and formatter paths.

## Isolated verification

A shell build with `-DZITH_HAS_LLVM=OFF` compiles `test-formatter` but cannot
link `zithc`, because `src/cli/cmd/run.cpp` references the intentionally
excluded `src/ir/hir-to-ir.cpp` and `src/interp/ir-vm.cpp` symbols. Use the
`test-*` targets for focused checks and do not claim the CLI build unless LLVM
is enabled or the ir/interp files are present.

Dependencies for an offline CMake build need local source copies:

```bash
cmake -S /home/diogo/Zith/.awt/agent7 \
      -B /tmp/zwork-agent7-build \
      -DZITH_HAS_LLVM=OFF \
      -DZITH_ENABLE_C_INTEROP=OFF \
      -DZITH_ENABLE_C_COMPILE=OFF \
      -DFETCHCONTENT_SOURCE_DIR_MIO=/tmp/zwork-agent7-mio \
      -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=/home/diogo/Zith/build/_deps/tomlplusplus-src
```

The full `fmt-check` target reports unrelated pre-existing formatting
violations in other test files; run `clang-format --dry-run --Werror` on the
edited sources instead.

