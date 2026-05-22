# Documentation Review & Improvement Plan

## Overview

Audit of all `.md` documentation files (73 total) for cross-document inconsistencies, terminology drift from the spec (`Zith-spec.md`), structural issues, and code example bugs.

## Priority 0 — FAQ Keyword Corrections (`mut` → `lend`, `let mut` → `var`)

The spec defines `lend` as the mutable borrow keyword and `var` for mutable variables. Four FAQ files use `mut` and `let mut` instead.

| File | Changes |
|---|---|
| `docsaurus/docs/faq/02-philosophy.md` | `mut self` → `lend self`; `mut_ref: mut [i32]` → `lend [i32]`; add `share`, `extension` to ownership summary |
| `docsaurus/docs/faq/03-security.md` | `mut i32` → `lend i32`; `fn process(data: mut [i32])` → `lend [i32]`; add `share`, `extension` |
| `docsaurus/docs/faq/04-rust-comparison.md` | `fn process(data: mut [i32])` → `lend [i32]`; `let mut scores` → `var scores` |
| `docsaurus/docs/faq/05-use-cases.md` | `let mut buffer` → `var buffer`; `file.read(mut buffer)` → `file.read(lend buffer)` |

## Priority 1 — Structural Issues

| Action | Details |
|---|---|
| **Remove duplicate CLI pages** | `cli/02-overview.md` is identical to `cli/check.md`; `cli/03-overview.md` is identical to `cli/run.md`. Delete the `02-` and `03-` copies. |
| **Add Docusaurus frontmatter** | `reference/stdlib.md` lacks `--- id: ... title: ... ---` block — won't appear in sidebar navigation. |
| **Translate Portuguese → English** | 6 files: `examples/README.md`, `examples/start/README.md`, `examples/dev/README.md`, `include/README.md`, `include/LLVM/README.md`, `tests/modules/README.md` |
| **Remove stale banner** | `cli/flags.md:11-13` says "under construction" despite complete content |
| **Document `zith execute`** | Referenced in `README.md` and quickstart but no CLI doc page exists for it |

## Priority 2 — Code Example Fixes

| File | Issue | Fix |
|---|---|---|
| `quickstart/01-hello-world.md:128` | `println("Count: {counter}"` missing `)` | Add closing `)` |
| `cli/repl.md:126` | Broken backtick `**[zith run\`](./run.md)**` | Fix backtick placement |

## Priority 3 — Consistency Nits

| Issue | Files | Action |
|---|---|---|
| Method call syntax: `Vec::new()` vs `Vec<T>.new()` vs `.new()` | `stdlib.md`, FAQ files | Pick one convention |
| Module path separator: `std/io/console` vs `std::thread` vs `std.io.console` | `stdlib.md` | Align to single convention |
| `panic!` vs `panic` | `intrinsics.md` vs `Zith-spec.md` | Align to `panic!` (spec) |
| Context activation: `use context SQL {}` vs `context WebApp {}` | intro vs modules docs | Align syntax |
| Ownership summaries list 3/5 keywords | FAQ files | Add `share`, `extension` |

## Confirmed Correct (no change needed)

- `from std/io/console;` in quickstart — valid distinct syntax (`from` injects symbols, `import` uses qualified names). Both keywords exist in lexer + parser.
- `mut self` (consumed-object pattern) — semantically distinct from `lend` (borrow). FAQ uses `mut` incorrectly as a borrow keyword, but `mut self` for consumption is a valid planned pattern.

## Cross-Reference Summary

| Doc Category | Spec Alignment | Code Example Quality | Structural Health |
|---|---|---|---|
| `Zith-spec.md` | — (reference) | Good | Good |
| `docsaurus/docs/language/` | **Good** (matches spec) | Good | Good |
| `docsaurus/docs/faq/` | **Poor** (`mut` for `lend`) | Some issues | Good |
| `docsaurus/docs/quickstart/` | Good (except `from`/`import` clarification) | Minor bugs | Good |
| `docsaurus/docs/cli/` | N/A (tooling) | OK | **Duplicate pages** |
| `docsaurus/docs/advanced/` | OK | OK | Good |
| `impl/**/README.md` | N/A (impl docs) | OK | **Portuguese files** |
| `tests/**/README.md` | N/A (test docs) | OK | **Portuguese files** |
| `examples/**/README.md` | N/A (example docs) | OK | **Portuguese files** |
| `include/**/README.md` | N/A (API docs) | OK | **Portuguese files** |

## Execution Order

1. P0 — FAQ keyword corrections (4 files)
2. P1 — Remove duplicate CLI pages, add frontmatter, translate Portuguese files, fix banners
3. P2 — Code example typo fixes
4. P3 — Style/convention alignment
