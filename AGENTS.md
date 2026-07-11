# Repository Guidelines

## Project Structure & Module Organization

Zith is a C++23 compiler. Compiler subsystems live under `src/`; keep code in the
directory matching its namespace, such as `zith::parser` in `src/parser/`.
Important areas include `ast/`, `lexer/`, `parser/`, `symbols/`, `types/`,
`sema/`, `hir/`, `codegen/`, and `session/`. `src/session/compilation-session.*`
orchestrates the pipeline. Unit tests are standalone executables in `tests/`.
The language standard library is in `stdlib/`, examples in `examples/`, and
user-facing language documentation in `docs/`.

## Build, Test, and Development Commands

Configure and build from the repository root:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Use `./build/zithc --help` to inspect the local CLI. A useful compiler smoke
test is `./build/zithc --include stdlib check examples/hello-world.zith`.
LLVM is optional: CMake disables native code generation when it is unavailable.
Use `cmake --build build --target fmt` to format sources, or
`cmake --build build --target fmt-check` to verify formatting without edits.

## Coding Style & Naming Conventions

Follow `.clang-format`: four spaces, no tabs, 100-column limit, attached braces,
and right-aligned pointers. Use `clang-format` rather than manually reformatting
unrelated code. Names use existing C++ conventions: classes and structs in
`PascalCase`, functions and fields in `camelCase`/`snake_case` as established by
the local module. Preserve arena-backed ownership patterns (`memory::Arena`,
`DynArray`) and do not introduce exceptions or RTTI.

## Testing Guidelines

Add focused coverage in `tests/test-<area>.cpp` and register the executable in
the root `CMakeLists.txt` through `add_zith_test`. Keep tests deterministic and
exercise both accepted input and diagnostics when changing parsing, imports, or
semantic analysis. Run the full CTest suite before submitting.

## Commit & Pull Request Guidelines

Recent history uses concise conventional prefixes when useful, for example
`feat: parser support for extern fn declarations`, `fix: ...`, `refactor: ...`,
and `build: ...`. Keep each commit narrowly scoped. Pull requests should explain
the compiler behavior changed, list tests run, link relevant issues, and include
sample source or output when diagnostics, syntax, or CLI behavior changes.
