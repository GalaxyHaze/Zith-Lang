---
id: getting-started-why-zith
title: Why Zith
section: Getting Started
output: getting-started/D-why-zith.html
aliases: intro/D-why-zith.html
kind: editorial
---
# Why Zith

Zith explores a systems-language design that combines a small everyday core
with explicit tools for domain-specific work. The working compiler already
gives you typed functions, algebraic data types, modules, traits, C interop,
generic instantiation, and LLVM or WebAssembly lowering.

## Why the split matters

Zith keeps the language design ahead of the compiler without pretending that
planned features are already stable. The [Zith-- documentation](doc:zith-subset-overview)
teaches the subset that `zithc` can compile today. The [Zith reference](doc:zith-overview)
describes the larger language model. [Implementation Status](doc:reference-implementation-status)
records the boundary feature by feature.

## A practical starting point

Use Zith-- when you want:

- explicit types and predictable conversions;
- low-level access through pointers, `raw opaque`, and C interop;
- generic data structures without a separate runtime type system;
- traits and interfaces for shared behaviour;
- a compiler pipeline that can target native code or WebAssembly.

Start with the [Quick Start](doc:getting-started-quick-start), then read the
[Zith-- Language Guide](doc:guide-overview). Move to the full specification
when you need to understand a feature that the current subset does not expose.
