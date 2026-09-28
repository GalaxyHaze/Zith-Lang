---
id: zith-overview
title: Zith Overview
section: Zith
output: zith/D-overview.html
aliases: language/D-zith.html
kind: editorial
---
# Zith Overview

Zith is the full language design. It documents the language model that the
project is moving toward, while `Zith--` is the subset currently built by the
main compiler.

## Which documentation should you read?

Read the [Zith-- documentation](doc:zith-subset-overview) when you are writing
code for `zithc`. It describes the syntax and behaviour that the current
compiler can verify.

Read the [Zith Language Reference](doc:reference-specification) when you are
designing a new language feature, studying the intended semantics, or comparing
the implemented subset with the larger language.

The full reference is not a promise that every feature is available in the
current compiler. The [Implementation Status](doc:reference-implementation-status)
page records the boundary between the two.

## How the split works

The website uses two layers:

| Layer | Audience | Source of truth |
| --- | --- | --- |
| Zith-- | People writing programs today | `docs/Zith--.md` and the practical guide |
| Zith | Language design and future features | `docs/Zith-spec-full.md` and its chapters |

The full specification may describe a feature in detail even when Zith-- only
parses it, rejects it with a diagnostic, or does not implement it at all. Each
practical page calls out that boundary instead of presenting planned behaviour
as if it were available.

## A stable way to learn the language

1. Start with the [Zith-- overview](doc:zith-subset-overview).
2. Follow the [Language Guide](doc:guide-overview) for task-oriented examples.
3. Check [Implementation Status](doc:reference-implementation-status) before
   using a feature marked experimental.
4. Use the formal [Zith reference](doc:reference-specification) for exact
   contracts and future language design.
