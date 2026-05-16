---
id: clean
title: zith clean
sidebar_label: clean
description: Remove build artifacts.
---

# `zith clean`

Remove all build artifacts and cached files.

## Usage

```bash
zith clean [options]
```

## Examples

### Clean Current Project

```bash
# Remove all build artifacts
zith clean
```

### Clean Specific Target

```bash
# Clean only debug builds
zith clean --profile dev

# Clean only release builds
zith clean --profile release
```

## Options

| Flag | Description |
|------|-------------|
| `--profile <name>` | Clean specific profile (dev, release) |
| `--all` | Clean everything including cache |
| `--verbose` | Show what's being deleted |

## What Gets Deleted

```
target/
├── debug/         # deleted
├── release/       # deleted
└── .cache/        # deleted (with --all)
```

## Cache Files

With `--all`, also removes:

- Incremental compilation cache
- Generated documentation
- Downloaded dependencies

## Tips

- Run `zith clean` when troubleshooting build issues
- Use `--all` to do a complete reset
- Combine with build: `zith clean && zith build`

## See Also

- [`zith build`](./build.md) - Build project
- [`zith check`](./check.md) - Check code