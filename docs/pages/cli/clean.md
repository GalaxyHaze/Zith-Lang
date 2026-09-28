---
id: cli-clean
title: zithc clean
section: CLI Reference
output: cli/D-clean.html
aliases: cli/D-clean.html
kind: editorial
---
# `zithc clean`

Remove generated build output and the compiler cache from the current project.

```bash
zithc clean
```

Run this when generated artifacts are no longer needed or when you need a fresh local build.

## When to use it

Run `clean` after changing target options, switching compiler builds, or
debugging a stale cache result:

```bash
zithc clean
zithc check
```

The command does not remove source files, project configuration, or the
installed standard library. It only removes the project build directory and
`.zith-cache`.
