---
id: fmt
title: zith fmt
sidebar_label: fmt
description: Format Zith code automatically.
---

# `zith fmt`

Format your Zith code according to the standard style guide.

## Usage

```bash
zith fmt [options] [files...]
```

## Examples

### Format Current Project

```bash
# Format all .zith files in project
zith fmt

# Format specific file
zith fmt src/main.zith
```

### Check Without Formatting

```bash
# Check if files need formatting (exit non-zero if they do)
zith fmt --check
```

### Format with Config

```bash
# Use specific config file
zith fmt --config .zithfmt.toml
```

## Options

| Flag | Description |
|------|-------------|
| `--check` | Check formatting without modifying |
| `--config <path>` | Use specific config file |
| `--write` | Write changes (default: yes) |
| `--verbose`, `-v` | Show what was changed |

## Configuration

Create `.zithfmt.toml` in project root:

```toml
[format]
indent = 4           # spaces per indent
line_width = 100     # max line length
quote_style = "double"  # "double" or "single"
```

## Exit Codes

- `0` - Files already formatted (or formatted with --check)
- `1` - Files needed formatting
- `2` - Error

## Tips

- Run `zith fmt` before committing
- Add to pre-commit hook
- Use `--check` in CI to enforce formatting

## See Also

- [`zith check`](./check.md) - Check for errors
- [`zith new`](./new.md) - Create new project