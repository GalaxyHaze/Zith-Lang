---
id: build
title: zith build
sidebar_label: build
description: Build the project with full pipeline.
---

# `zith build`

The `build` command runs the full build pipeline: check, compile, and link.

## Usage

```bash
zith build [options] [project]
```

## Examples

### Build Current Project

```bash
# Build with default settings
zith build

# Build in release mode
zith build --release

# Build with specific target
zith build --target x86_64-unknown-linux-gnu
```

### Build Options

```bash
# Enable all warnings as errors
zith build --warnings-as-errors

# Specify output directory
zith build --out-dir ./dist

# Parallel build
zith build -j 4
```

## Options

| Flag | Description |
|------|-------------|
| `--release` | Build in release mode |
| `--debug` | Build in debug mode |
| `--target <triple>` | Target triple |
| `--out-dir <path>` | Output directory |
| `--warnings-as-errors` | Treat warnings as errors |
| `-j <n>` | Parallel jobs |
| `--verbose` | Verbose output |

## Build Profiles

### Debug (default)

```toml
[profile.dev]
debug_symbols = true
optimize = false
incremental = true
```

### Release

```toml
[profile.release]
optimize = "speed"
lto = true
strip_symbols = true
```

## Output

Build creates:

```
target/
├── debug/
│   └── my_project (or my_project.exe)
└── release/
    └── my_project
```

## Incremental Builds

Zith caches compilation results for faster rebuilds:

```bash
# First build
zith build        # takes time

# Second build (incremental)
zith build        # fast, only rebuilds changed files
```

## Cleaning Build

```bash
# Remove build artifacts
zith clean

# Clean and rebuild
zith clean && zith build
```

## Tips

- Use `--release` for production builds
- Use `--verbose` to see what's being compiled
- Run `zith check` first to catch errors quickly

## See Also

- [`zith compile`](./compile.md) - Compile without linking
- [`zith run`](./run.md) - Build and run
- [`zith clean`](./clean.md) - Clean build artifacts