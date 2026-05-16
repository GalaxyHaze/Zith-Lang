---
id: compile
title: zith compile
sidebar_label: compile
description: Compile Zith code to a binary.
---

# `zith compile`

Compile your Zith code to an executable binary without linking.

## Usage

```bash
zith compile [options] [file|project]
```

## Examples

### Compile Current Project

```bash
# Compile to default output
zith compile

# Specify output name
zith compile -o my_binary
```

### Compile Specific File

```bash
# Compile single file to object file
zith compile src/main.zith -o main.o

# Compile with debug info
zith compile src/main.zith -g
```

## Options

| Flag | Description |
|------|-------------|
| `-o <path>` | Output file path |
| `-g` | Include debug symbols |
| `--target <triple>` | Target triple (e.g., x86_64-unknown-linux-gnu) |
| `--opt <level>` | Optimization level (0-3, s, z) |
| `--emit <type>` | Emit type (exe, obj, llvm-ir, asm) |

## Output Types

### Executable (default)

```bash
zith compile -o myapp
# Creates myapp (or myapp.exe on Windows)
```

### Object File

```bash
zith compile --emit obj -o main.o
# Creates object file for linking
```

### LLVM IR

```bash
zith compile --emit llvm-ir -o main.ll
# Creates LLVM IR for analysis
```

### Assembly

```bash
zith compile --emit asm -o main.s
# Creates assembly output
```

## Optimization Levels

| Level | Description |
|-------|-------------|
| 0 | No optimization (fastest compile) |
| 1 | Basic optimizations |
| 2 | Standard optimizations (default) |
| 3 | Aggressive optimizations |
| s | Optimize for size |
| z | Optimize for size (aggressive) |

## Cross Compilation

```bash
# Compile for different target
zith compile --target aarch64-unknown-linux-gnu
```

## Tips

- Use `-O2` for development, `-O3` or `-Os` for release
- Use `--emit llvm-ir` to inspect generated code
- Use `-g` for debugging

## See Also

- [`zith build`](./build.md) - Full build pipeline
- [`zith run`](./run.md) - Compile and run
- [`zith check`](./check.md) - Check without compiling