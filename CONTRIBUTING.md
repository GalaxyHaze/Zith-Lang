# Contributing

## Build

```bash
cmake -B build
cmake --build build
```

Requires: CMake 3.20+, C++23 compiler (Clang 18+ or GCC 14+).

## Code Style

- Follow LLVM style (4-space indent, 100 columns, attach braces)
- Use `#pragma once` for headers
- Use `snake_case` for files, types in `PascalCase`, functions in `camelCase`
- Namespace format: `zith::layer::module`
- Prefer arena allocation over `new`/`delete`

## Testing

```bash
cmake --build build          # builds all test targets
./build/zith-lexer-test
./build/zith-parser-expr
./build/zith-parser-stmt
./build/zith-sema-test
./build/zith-mir-test
```

## Pull Requests

- Keep commits focused and rebased on `rewrite`
- Write descriptive commit messages
- Ensure all tests pass before opening
