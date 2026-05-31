# CLI Driver

Command-line interface and compilation pipeline orchestration.

## Structure

```
cli/
├── CLI.cpp                     # CLI entry point (CLI11 subcommand dispatch)
├── cmd/                        # Command implementations
│   ├── commands.hpp            # All command declarations (zith::cli::commands)
│   ├── build/                  # Compilation pipeline commands
│   │   └── build.cpp           #   cmd_check, cmd_compile, cmd_build
│   ├── run/                    # Execution commands
│   │   └── run.cpp             #   cmd_execute, cmd_run
│   ├── project/                # Project management commands
│   │   └── project.cpp         #   cmd_new, cmd_clean
│   ├── tool/                   # Development tool commands
│   │   └── tool.cpp            #   cmd_test, cmd_fmt, cmd_docs
│   ├── interactive/            # Interactive commands
│   │   └── interactive.cpp     #   cmd_repl
│   └── info/                   # Information commands
│       └── info.cpp            #   cmd_version, cmd_help
├── pipeline/                   # Pipeline utilities (tokenization, print helpers)
├── project-config/             # ZithProject.toml parsing
└── runtime-interpreted/        # Bytecode interpreter runtime
```

## Entry Point

```
main.cpp → zith_run() → CLI.cpp → zith::cli::commands::*
```

## Command-Line Interface

```bash
# Build a project
zith build
zith build -m release

# Run a file
zith run file.zith
zith run file.zt
zith run file.z

# Compile (no linking)
zith compile file.zith
zith compile --interpreted file.zith -o main.nbc

# Execute existing binary/bytecode
zith execute
zith execute --interpreted

# Type check
zith check file.zith

# Format
zith fmt file.zith
zith fmt file.zith --check

# Start REPL
zith repl

# Generate docs
zith docs file.zith -o docs/

# Clean build artifacts
zith clean

# New project
zith new project_name
```

## Project Configuration

Reads `ZithProject.toml` for project settings:

```toml
[project]
name = "myproject"
version = "0.1.0"
description = "My project"
authors = ["Author Name"]
license = "MIT"

[build]
entry = "src/main.zith"
output = "bin/myproject"
mode = "debug"
edition = "2024"
opt_level = 0
debug_level = 2

[paths]
src_dir = "src"
bin_dir = "target"
mod_dir = ".zmodules"
docs_dir = "docs"
test_dir = "test"
cache_dir = ".zcache"
```

## Pipeline Orchestration

The CLI orchestrates the full compilation:

```cpp
// 1. Load source or ZithProject.toml
std::string source = load_input(cli.input);

// 2. Tokenize
ZithTokenStream stream;
ZithArena *arena = tokenize_file(src, stream, &source, &src_size, verbose);

// 3. Parse to AST
ZithNode *ast = zith_parse_with_source(arena, source, src_size, src.c_str(), stream, ...);

// 4. Generate LLVM IR / bytecode
//   - Native: LLVM codegen → .o → link → binary
//   - Interpreted: bytecode emit → .nbc → run

// 5. Emit executable or library
```

## Options

| Flag | Description |
|------|-------------|
| `-m`, `--mode` | Build mode: debug, dev, release, fast, test |
| `-o`, `--output` | Output file/path |
| `-I`, `--include` | Include directories (repeatable) |
| `--interpreted` | Use bytecode path instead of native |
| `--emit` | Emit intermediate representation (ast, ir, asm, obj, bin) |
| `--target` | Target triple |
| `-v`, `--verbose` | Verbose output |

## Dependencies

- **CLI11** — Command-line argument parsing
- **tomlplusplus** — TOML parsing for ZithProject.toml
- **LLVM** — Code generation and JIT (optional)

## Integration

- Uses `lexer/` for tokenization
- Uses `parser/` for AST generation
- Uses `diagnostics/` for error output
- Uses `memory/` for arena allocation

## See Also

- `docsaurus/` — Documentation generator
- `include/` — Public headers
