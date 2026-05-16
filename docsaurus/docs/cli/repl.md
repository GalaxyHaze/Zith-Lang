---
id: repl
title: zith repl
sidebar_label: repl
description: Interactive REPL for experimenting with Zith.
---

# `zith repl`

Start an interactive Read-Eval-Print Loop (REPL) for experimenting with Zith code.

## Usage

```bash
zith repl [options]
```

## Examples

### Start REPL

```bash
# Start default REPL
zith repl
```

### REPL with Options

```bash
# Enable verbose mode
zith repl --verbose

# Load custom prelude
zith repl --prelude my_prelude.zith
```

## Options

| Flag | Description |
|------|-------------|
| `--verbose` | Show evaluation details |
| `--prelude <path>` | Load custom prelude |
| `--no-colors` | Disable syntax highlighting |

## Using the REPL

### Basic Evaluation

```
> let x = 42
x: i32 = 42
> x * 2
i32 = 84
```

### Multi-line Input

```
> fn add(a: i32, b: i32): i32 {
.     a + b
. }
add: fn(i32, i32) -> i32
> add(1, 2)
i32 = 3
```

### Help

```
> :help
Available commands:
  :help    - Show this help
  :quit    - Exit REPL
  :clear   - Clear screen
  :load    - Load file
  :type    - Show expression type
```

### Show Type

```
> :type 42
i32

> :type "hello"
[]char
```

### Load File

```
> :load examples/test.zith
Loaded examples/test.zith
```

## REPL Commands

| Command | Description |
|---------|-------------|
| `:help` | Show help |
| `:quit` | Exit REPL |
| `:clear` | Clear screen |
| `:load <path>` | Load Zith file |
| `:type <expr>` | Show type of expression |
| `:history` | Show command history |
| `:ast <expr>` | Show AST of expression |

## Features

- **Syntax highlighting** - Colorized code
- **Auto-complete** - Tab completion for identifiers
- **History** - Arrow keys for command history
- **Multi-line** - Brace matching for blocks
- **Error display** - Clear error messages with location

## Tips

- Use `Tab` for auto-completion
- Use `Ctrl+C` to cancel input
- Use `Ctrl+D` to exit
- Pre-load utilities with `--prelude`

## See Also

- **[Quickstart](../quickstart/01-hello-world.md)** - Writing first program
- **[zith run`](./run.md)** - Run program file