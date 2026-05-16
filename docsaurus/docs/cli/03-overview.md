---
id: run-command
title: Run Command
sidebar_label: run
description: Compile and run Zith programs directly.
---

# `zith run`

The `run` command compiles and executes your Zith program in a single step. Ideal for quick testing and development.

## Usage

```bash
zith run [options] [file|project]
```

## Examples

### Run Current Project

```bash
# Run project from root directory
zith run
```

### Run Specific File

```bash
# Run a single file
zith run src/main.zith

# Run with arguments
zith run src/script.zith -- arg1 arg2
```

### Run with Debug Mode

```bash
# Enable debug output
zith run --debug src/main.zith
```

## Options

| Flag | Description |
|------|-------------|
| `--debug`, `-d` | Enable debug output |
| `--release` | Run in release mode (optimized) |
| `--args <args>` | Pass arguments to program |
| `--watch` | Re-run on file changes |

## Exit Codes

- `0` - Program executed successfully
- `1` - Program exited with error
- `2` - Compilation error

## How It Works

1. **Compile** - Runs `zith check` to verify code
2. **Build** - Compiles to temporary binary
3. **Execute** - Runs the compiled program

## Tips

- Use `--release` for performance testing
- Use `--watch` during development for automatic re-runs
- Pass `--` followed by args to send to your program

## See Also

- [`zith build`](./build.md) - Full build pipeline
- [`zith compile`](./compile.md) - Generate binary only
- [`zith check`](./02-overview.md) - Check without running