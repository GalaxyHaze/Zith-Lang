# Documentation Review & Improvement Plan

## Overview

Audit of all `.md` documentation files (73 total) for cross-document inconsistencies, terminology drift from the spec (`Zith-spec.md`), structural issues, and code example bugs.

## Keyword Terminology Summary

Zith has three distinct uses of keywords — be careful not to conflate them:

| Context | Correct Keyword | Examples |
|---|---|---|
| Mutable variable declaration | `var` | `var x = 0;` |
| Mutable borrow (temporary) | `lend` | `fn foo(x: lend i32)` |
| Consumption (owned, mutable self) | `mut self` | `fn foo(mut self: Health)` |
| ECS dynamic entity | `mut` after `entity` | `entity mut GameEntity {}` |
| Mutable raw pointer type | `*mut T` | `let p: *mut u8 = ...` |
| Dynamic entity argument | `mut entity` | `@registerComponent(mut entity, ...)` |

**Rule of thumb:** If `mut` is used as a standalone borrow/variable keyword → wrong. If paired with `entity`, `self`, or `*` → check context.

## Priority 0 — FAQ Keyword Corrections (`mut` → `lend`, `let mut` → `var`)

The spec defines `lend` as the mutable borrow keyword and `var` for mutable variables. Several files use `mut` and `let mut` incorrectly.

### Confirmed Correct (leave as-is)

- `mut self: Health` in `faq/02-philosophy.md:34` and `intro/why-zith.md:32` — consumption pattern, **NOT a borrow**
- `entity mut GameEntity` in `ecs.md:62` — ECS dynamic entity, matches spec
- `@registerComponent(mut entity, ...)` in `intrinsics.md:53` — ECS context, matches spec
- `*mut T` / `*mut u8` / `*mut u32` in raw pointer docs — correct raw pointer syntax
- `let mut scores` on line 204 of `faq/04-rust-comparison.md` — inside a Rust code block, should stay

### Must Fix

#### FAQ Files

| File | Line | Current | Should Be |
|---|---|---|---|
| `docsaurus/docs/faq/02-philosophy.md` | 64 | `let mut_ref: mut [i32]` | `let mut_ref: lend [i32]` |
| `docsaurus/docs/faq/03-security.md` | 27 | `### Three Ownership Modifiers` | `### Five Ownership Modifiers` |
| `docsaurus/docs/faq/03-security.md` | 37-38 | `// mut - Mutable borrow` / `let mut_ref: mut i32` | `// lend - Mutable borrow` / `let mut_ref: lend i32` |
| `docsaurus/docs/faq/03-security.md` | 93 | `fn process(data: mut [i32])` | `fn process(data: lend [i32])` |
| `docsaurus/docs/faq/03-security.md` | 174 | `view where mut needed?` | `view where lend needed?` |
| `docsaurus/docs/faq/04-rust-comparison.md` | 40 | `fn process(data: mut [i32])` | `fn process(data: lend [i32])` |
| `docsaurus/docs/faq/04-rust-comparison.md` | 72 | `mut i32` (error example) | `lend i32` |
| `docsaurus/docs/faq/04-rust-comparison.md` | 221 | `let mut scores = Map...` | `var scores = Map...` |
| `docsaurus/docs/faq/05-use-cases.md` | 61 | `let mut buffer: unique [u8]` | `var buffer: unique [u8]` |
| `docsaurus/docs/faq/05-use-cases.md` | 63 | `file.read(mut buffer)` | `file.read(lend buffer)` |

#### Non-FAQ Files

| File | Line | Current | Should Be |
|---|---|---|---|
| `docsaurus/docs/language/01-overview.md` | 22 | `"unique", "view", "mut"` | `"unique", "share", "view", "lend", "extension"` |
| `docsaurus/docs/language/01-overview.md` | 63 | `unique, view, mut` | `unique, share, view, lend, extension` |

### P0.b — `self: mut` Syntax Order Fix

| File | Line | Current | Should Be |
|---|---|---|---|
| `docsaurus/docs/intro/01-overview.md` | 38 | `fn process(self: mut Health, dmg: view u16)` | `fn process(mut self: Health, dmg: view u16)` |

## Priority 1 — Structural Issues

| Action | Details |
|---|---|
| **Remove duplicate CLI pages** | `cli/02-overview.md` is a copy of `cli/check.md`; `cli/03-overview.md` is a copy of `cli/run.md`. Delete the `02-` and `03-` copies. |
| **Add Docusaurus frontmatter** | `reference/stdlib.md` lacks `--- id: ... title: ... ---` block — won't appear in sidebar navigation. |
| **Translate Portuguese → English** | 6 files: `examples/README.md`, `examples/start/README.md`, `examples/dev/README.md`, `include/README.md`, `include/LLVM/README.md`, `tests/modules/README.md` |
| **Remove stale banner** | `cli/flags.md:11-13` says "under construction" despite complete content |
| **Document `zith execute`** | Referenced in `README.md` and quickstart but no CLI doc page exists for it |

## Priority 2 — Code Example Fixes

| File | Line | Issue | Fix |
|---|---|---|---|
| `quickstart/01-hello-world.md` | 128 | `println("Count: {counter}"` missing `)` | Add closing `)` |
| `cli/repl.md` | 126 | Broken backtick `**[zith run\`](./run.md)**` | Fix backtick placement |

## Priority 3 — Consistency Nits

| Issue | Files | Action |
|---|---|---|
| Method call syntax: `Vec::new()` vs `Vec<T>.new()` vs `.new()` | `stdlib.md`, FAQ files | Pick one convention |
| Module path separator: `std/io/console` vs `std::thread` vs `std.io.console` | `stdlib.md` | Align to single convention |
| `panic!` vs `panic` | `intrinsics.md` vs `Zith-spec.md` | Align to `panic!` (spec) |
| Context activation: `use context SQL {}` vs `context WebApp {}` | intro vs modules docs | Align syntax |
| Ownership summaries list 3/5 keywords | FAQ files + `language/01-overview.md` | Add `share`, `extension` |

## Confirmed Correct (no change needed)

- `from std/io/console;` in quickstart — valid distinct syntax (`from` injects symbols, `import` uses qualified names). Both keywords exist in lexer + parser.
- `mut self` (consumed-object pattern) — semantically distinct from `lend` (borrow). FAQ uses `mut` incorrectly as a borrow keyword, but `mut self` for consumption is valid.
- `entity mut` in ECS — correct per spec for dynamic entities
- `*mut T` in raw pointers — correct raw pointer syntax

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

1. P0 — FAQ keyword corrections (5 files + 1 non-FAQ)
2. P0.b — `self: mut` syntax fix (1 file)
3. P1 — Remove duplicate CLI pages, add frontmatter, translate Portuguese files, fix banners
4. P2 — Code example typo fixes
5. P3 — Style/convention alignment
