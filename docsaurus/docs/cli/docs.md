---
id: docs
title: zith docs
sidebar_label: docs
description: Generate documentation from code.
---

# `zith docs`

Generate documentation from Zith source code and doc comments.

## Usage

```bash
zith docs [options] [project]
```

## Examples

### Generate Docs

```bash
# Generate docs for current project
zith docs

# Open in browser after generation
zith docs --open
```

### Docs Configuration

```bash
# Use specific config
zith docs --config docs.toml
```

## Options

| Flag | Description |
|------|-------------|
| `--output <path>` | Output directory (default: doc/) |
| `--open` | Open docs in browser after generation |
| `--format <type>` | Format (html, markdown) |
| `--verbose` | Show generated files |

## Doc Comments

Add documentation with `///` or `/** */`:

```zith
/// Calculates the sum of two numbers.
///
/// # Arguments
/// * `a` - First number
/// * `b` - Second number
///
/// # Returns
/// The sum of a and b
///
/// # Example
/// ```zith
/// let result = add(1, 2);  // result is 3
/// ```
fn add(a: i32, b: i32): i32 {
    a + b
}
```

### Sections

- `# Arguments` - Parameter descriptions
- `# Returns` - Return value description
- `# Example` - Usage examples
- `# See Also` - Related items
- `# Notes` - Additional notes

## Module Documentation

```zith
/// This module contains utility functions
/// for string manipulation.
mod utils {
    // ...
}
```

## Output

Generated documentation structure:

```
doc/
├── index.html          # Main page
├── module_name/
│   └── function.html  # Per-item docs
└── styles.css
```

## Markdown Output

```bash
# Generate markdown instead of HTML
zith docs --format markdown
```

Output:

```
docs/
├── README.md
├── module_name.md
└── function.md
```

## Tips

- Document public APIs
- Include examples in doc comments
- Run `zith docs --open` to preview

## See Also

- [`zith build`](./build.md) - Build project
- **[Documentation](../intro/01-overview.md)** - Docs overview