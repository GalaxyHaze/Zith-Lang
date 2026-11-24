---
id: getting-started-quick-start
title: First Program
section: Zith--
output: zith--/guide/D-first-program.html
aliases: D-quickstart.html
kind: editorial
---
# First Program

Zith-- is in beta. This guide uses the working `zithc create`, `build`, and
`run` commands described in [Implementation Status](doc:reference-implementation-status).
The beta supports Linux x86-64 with the GNU runtime. Read the
[Beta Support And Limitations](doc:getting-started-beta-support) table before
you rely on another platform.

## Create a project

```bash
zithc create hello-zith
cd hello-zith
```

`create` writes a `ZithProject.toml` and `src/main.zithm` scaffold.

## Write a program

Replace `src/main.zithm` with a small program:

```zith
fn add(a: i32, b: i32): i32 { a + b }

fn main() {
    let total = add(20, 22);
}
```

## Build and run

```bash
zithc check
zithc build
zithc run
```

`check` type-checks without emitting output. `run` compiles and executes in one step. Continue with the [Introduction](doc:getting-started-introduction) for the documentation map, or read [Syntax](doc:guide-syntax) before expanding the program.

To work in VS Code, follow [Editor Setup](doc:getting-started-editor-setup). The
standard-library reference in [Standard Library](doc:reference-20-standard-library)
lists the stable `std/` modules, the raw `c/` bindings, and the experimental
`soon/` modules.
