---
id: new
title: zith new
sidebar_label: new
description: Create a new Zith project.
---

# `zith new`

Create a new Zith project with a basic template.

## Usage

```bash
zith new <project_name> [options]
```

## Examples

### Create New Project

```bash
# Create project in current directory
zith new my_project

# Create project in specific directory
zith new my_project --path ./projects
```

### Create with Template

```bash
# Create with library template
zith new my_lib --template lib

# Create with binary template
zith new my_app --template bin
```

## Options

| Flag | Description |
|------|-------------|
| `--path <path>` | Create project in specific directory |
| `--template <name>` | Choose template (bin, lib, default) |
| `--force` | Overwrite if directory exists |

## Project Structure

Created project has this structure:

```
my_project/
├── ZithProject.toml
├── src/
│   └── main.zith
└── tests/
    └── test.zith
```

## ZithProject.toml

The created configuration file:

```toml
[project]
name = "my_project"
version = "0.1.0"
edition = "2024"
authors = ["Your Name <you@example.com>"]
description = "A new Zith project"

[dependencies]
# Add dependencies here
```

## Next Steps

- **[zith build](./build.md)** - Build the project
- **[zith run](./run.md)** - Run the program
- **[Project Configuration](../project/01-overview.md)** - Configure project