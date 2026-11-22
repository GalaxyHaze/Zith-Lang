---
id: getting-started-why-zith
title: Why Zith
section: Getting Started
output: getting-started/D-why-zith.html
aliases: intro/D-why-zith.html
kind: editorial
---

# Why Zith

Zith is an ambitious systems language that proposes a different philosophy:

**Learn only what you need, in your current domain**, while balancing ergonomics and explicitness.

Zith--, on the other hand, is a more straightforward language. While sharing most of the rich type system, it is a simplification of the overall Zith model and closer to other systems languages. Its philosophy is:

**Explicit where it matters, easy where it doesn't**.

## What is the distinction?

Zith and Zith-- have different proposals and goals. for example

- **Zith** has a richer error system (per-expression guards, resumable errors, acquire/initialization errors), a Scope DSL, compile-time contracts, ownership semantics and more, It aims to give you more power without forcing or expecting you to know the entire model.
- **Zith--** has native `?T` types and `?` propagation, heuristics for ownership, and an overall design simplification. It aims to be smaller, easier to learn completely, and immediately usable.

## Choosing a language

For now, choose **Zith--** if you want to write programs today and care about:

- Simplicity, explicit types and errors
- Seamless low-level access and C interop
- Straightforward generic data structures
- Familiar traits and interfaces for behaviour
> **note:** is the current implemented `zithc`

When **Zith** is released, choose it if you care about:

- The full rich type system and `comptime` power
- The expressive error system
- Scope DSL and compile-time contracts
- Full ownership semantics

> **note:**  
> Zith changes some common concepts already established by certain languages,
> whereas Zith-- is simpler and follows the standard.

Start with the [First Program](doc:getting-started-quick-start) and the [Zith-- Language Guide](doc:guide-overview).  
Read the Zith reference to study the design; its features do not automatically apply to Zith--.
