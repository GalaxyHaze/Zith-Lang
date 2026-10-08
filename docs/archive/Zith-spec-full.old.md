# ZITH Language Specification
**Draft v0.9 — 2026**

> **Status: archived.** This is a single-file aggregate of the numbered chapter
> docs (`docs/02-*.md` through `docs/21-*.md`). It is kept only as history and
> is not an active specification surface. The active compiler contract is
> [`docs/Zith--.md`](../Zith--.md); per-feature implementation status is
> [`docs/impl-status.md`](../impl-status.md).

> Reference document for the full Zith language surface, including features
> that are outside the Zith-- subset compiled by `main`. The active compiler
> contract is `docs/Zith--.md`; per-feature implementation status is
> `docs/impl-status.md`.

> Zith is a statically typed systems programming language with a small, composable core and a large toolbox for domain-specific work. It proves memory safety at compile time without a garbage collector or borrow checker.

## Introduction

Zith gives you full control with a minimal & clean syntax — you don't have to choose between verbose but safe or readable but slow. Its memory model, Node Resource Analysis (NRA), proves ownership and lifetime safety using `&T` references, `^T` binds, `%T` own values, and the implicit `default` form.

Beyond memory safety, Zith has a general-purpose core with a larger toolbox: state machines, contexts for domain-specific syntax, words (custom operators), and comptime. You choose when to use them. Zith also follows the **Rule of Three**: "if a function needs more than three specialized tools, something went wrong."

Zith also aims for a shared public-API style that remains familiar across
different implementation styles. A project's language identity is separate:
it controls optional features and local diagnostic rationale for the project's
own code, not the design of public APIs or the features used by dependencies.
See [ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

This document is a draft of the language specification, currently v0.9.
Not every feature described here is implemented in the compiler.
For the exact picture of what works today, see [Implementation Status](impl-status.md). It serves three audiences: developers learning Zith for the first time, contributors working on the `zithc` compiler, and tooling authors building editors, linters, or other infrastructure around the language.

### Notation used in this document

| Symbol | Meaning |
|---|---|
| `?T` | Deprecated optional type wrapper. Still accepted by the current Zith-- compiler as legacy syntax. |
| `T!` | Deprecated as a type wrapper. In a function return annotation, `T!` declares success type `T` and lets the compiler infer invalid states. |
| `try` / `or` | Error handling | Short-circuit one expression and provide fallbacks for any invalid state ([§8.3](08-error-handling.md#83-try-propagation-and-fallback)). |
| Postfix `?` | Legacy Zith-- syntax | Propagates `Nil`; its current compiler support is separate from the full-Zith model. |
| Postfix `!` | Error handling | Propagate an operation's invalid state to the enclosing function ([§8.3](08-error-handling.md#83-try-propagation-and-fallback)). |
| `@name` | Compiler intrinsic or compiler magic. Zith-- also uses it for macro calls. |
| `@<Tag>` | Dedicated full-Zith tag opener, closed by `</Tag>`. |
| `#name` | Variable or field attribute, e.g. `#thread_local` or `#volatile` |
| `::` | Scope resolution — reach past a shadowed name ([§2.3](02-module-system.md#23-namespace-access--scope-resolution)) |

This is a draft specification and remains subject to change as the compiler matures.

---

## Table of Contents

1. [Overview & Design Philosophy](#1-overview--design-philosophy)
2. [Module System](#2-module-system)
3. [Type System](#3-type-system)
4. [Traits, Interfaces & Capabilities](#4-traits-interfaces--capabilities)
5. [Functions](#5-functions)
6. [Mutability & Bindings](#6-mutability--bindings)
7. [Memory Model (NRA)](#7-memory-model-nra)
8. [Error Handling](#8-error-handling)
9. [Control Flow](#9-control-flow)
10. [Concurrency & Runtime APIs](#10-concurrency--runtime-apis)
11. [Comptime](#11-comptime)
12. [Assets](#12-assets)
13. [Raw & Unsafe](#13-raw--unsafe)
14. [Runtime: Polymorphism & Dynamic Behaviour](#14-runtime-polymorphism--dynamic-behaviour)
15. [Macros](#15-macros)
16. [Words](#16-words-custom-operators)
17. [Contexts](#17-contexts)
18. [C Interop](#18-c-interop)
19. [Project Configuration](#19-project-configuration)
20. [Standard Library](#20-standard-library)
21. [Best Practices & Patterns](#21-best-practices--patterns)
22. [Appendix — Keyword & Operator Reference](#22-appendix--keyword--operator-reference)

---

## 1. Overview & Design Philosophy

### 1.1 Who Zith Is For

If you are looking for just a 'new' language, clone or 'normal', so Zith is not for you. Zith was created for people starting to learn or open-minded, to discover new ways to think and structure your code, while having a powerful, readable, safe & expressive language.

### 1.2 Our Philosophy

Zith aims to be small and stable at its core — covering everyday needs — while offering a large kit that helps in specific domains where most languages need a lot of tricks to work.
The compiler is a copilot: it gives you the tools, and you build the systems.

| Everyday | Domain-specific |
|---|---|
| `struct`, `fn`, `&`, `&mut`, `trait`, `interface` | `state`, `dock`, `jump` — for Games, State Machine, OS & embedded |
| `?T`, `or` | `context`, `word` — for domain-specific syntax integration |
| `when`, `for`, `|>`/`do` | runtime/stdlib concurrency APIs — for parallel work without special syntax |

### 1.3 Design Goals

- Expressive, minimal syntax that favors readability without sacrificing power.
- Memory safety via Node Resource Analysis (NRA).
- Composable behavior through traits, capabilities, and interfaces.
- Static, zero-overhead error handling with rich recovery semantics.
- Compile-time computation (`comptime`) as a first-class feature.
- Low-level control — state functions and musttail state transitions — without sacrificing safety in everyday code.
- Domain-specific syntax integration through contexts, tags, and words. Contexts are not general-purpose API namespaces.

### 1.4 Domain-Specific Syntax Integration

Contexts are reserved for APIs that deliberately integrate with a domain's
syntax, such as SQL, HTML, or Math. They are not generic containers for public
API declarations or general-purpose syntax declarations. Prefer scoped
activation when a domain integration only applies to one block. Full-Zith tag
delimiters and body kinds are defined in
[ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md). Exact
context activation and tag declaration syntax remain under design.

```zith
// Domain-specific syntax, scoped to this block
use SQL {
    // SQL-specific forms are active here
}

// The current draft also permits activation in the surrounding scope
use SQL;
```

### 1.5 Compilation Pipeline

The `zithc` compiler follows a multi-stage pipeline:

```
source -> lex -> scan -> resolve(import/symbols) -> sema -> comptime/solve -> NTA/NRA -> HIR -> LLVM
```
> Note: when you compile a library, after LLVM it outputs `.zirl` (Zith Intermediate Representation Library).

| Stage | Description |
|---|---|
| `source` | Receive arguments from CLI, load the file |
| `lex` | Tokenize the file into a `TokenStream` |
| `scan` | Find top-level declarations from the token stream |
| `resolve` | Resolve imported symbols, report duplicates |
| `sema` | Semantic analysis — name resolution, type checking, visibility |
| `comptime/solve` | Reserved for future generic instantiation, `comptime` evaluation, and the solved semantic view. In Zith--, macro expansion currently happens during frontend parsing and uses the source AST directly. |
| `NTA/NRA` | Accumulate semantic/resource facts, prove ownership rules, emit diagnostics, and apply only internal canonicalizations that do not change public ABI |
| `HIR` | Build High-level IR — the typed, desugared program with residual ownership facts attached when available |
| `LLVM` | Code generation via the LLVM backend |

`.zirl` files serve as cache and distribution format for compiled libraries — no headers needed, OS-agnostic, and you choose static or dynamic linking at the client side. Distribute once, link however the consumer prefers.

---

## 2. Module System

### 2.1 Import Keywords

| Keyword | Behavior |
|---|---|
| `import path` / `import path as name` | Imports a module under its path as a namespace, or under an explicit alias. Members are accessed as `name.symbol` |
| `from path` | Injects all visible symbols directly into scope, sugar for simple cases |
| `export path` | Re-exports a dependency; consumers of this module receive it as well |

```zith
import std/io/console as console;
@console.println("hi");

from std/io/console;
@println("hi");

export std/io/console;
```

### 2.2 `alias` vs `use` vs `type`

These three keywords are easy to conflate but serve distinct purposes:

| Keyword | Purpose | Example |
|---|---|---|
| `alias` | Create a name alias for a type, namespace, or symbol. | `alias Vec = std.collections.DynArray;` |
| `type` | Creates a new distinct type from an existing one | `type Celsius = f32;` |
| `use` | Bring a word, context, or operator into the current scope. | `use math.vec.dot as DOT;` / `use SQL;` |

```zith
alias Vec   = std.collections.DynArray;
alias print = std.io.console.println;

use math.vec.dot as DOT;
use SQL;

type angle = f32;
type celsius = f32;
type uuid = u128;
```

### 2.3 Namespace Access & Scope Resolution

Namespaces are accessed with `.` — e.g. `std.io.console.println`. The `::` operator reaches upward past a shadowed name to the outer scope:

```zith
let x = 10;
{
    let x = 20;
    @println(::x);   // 10, outer scope
}
```

### 2.4 Type Constraints vs. Union Separators

Type constraints and union variants look similar but use different separators to avoid ambiguity:

| Construct | Separator | Semantics |
|---|---|---|
| Type constraint | `or` (keyword) | Compile-time restriction / constraint |
| Union body | `,` (comma) | Runtime-tagged union of member types, which may be heterogeneous. |

```zith
// Type constraint -- compile-time dispatch
type Number = i32 or f64 or bool;
fn convert<T: Number>(val: T): string { ... }

// Union by default is runtime tagged
union AnyNumber { i32, f64, bool }
```

### 2.5 Visibility

| Modifier | Scope |
|---|---|
| *(none)* | Private — visible only within the declaring file. |
| `pub` | Public — visible to any importer. |
| `mod` | Module-local — visible to immediate sibling files in the same directory. |
| `mod(..)` | Visible to all sub-directories, unlimited depth. |
| `mod(N)` | Visible to exactly N levels of sub-directories deep. |

---

## 3. Type System

### 3.1 Primitive Types

| Category | Types |
|---|---|
| Unsigned integers | `u8`, `u16`, `u32`, `u64`, `u128` |
| Signed integers | `i8`, `i16`, `i32`, `i64`, `i128` |
| Floats | `f32`, `f64` |
| Other primitives | `bool`, `char`, `void` |
| Compiler-internal | `unknown` — a valid but unresolved type, not user-instantiable. `invalid` — a dead or uninitialized state (a moved variable, a proven-null variable). Neither can be named or stored by user code. |
| Special | `never`, `null` |
| Opaque | `opaque` — a reference type (`&` by default), equivalent to a tagged `void*`. `raw opaque` is an untagged `void*`, used for C interop. |

### 3.2 Slice & Array Types

| Syntax | Meaning |
|---|---|
| `[]T` | Slice — a fat pointer (pointer + length). String literals are `[]char`. |
| `[N]T` | Fixed-size array, stack-allocated. |
| `[_]T` | Deduced-size array — the compiler infers `N` from the initializer. |

#### Strings & Origin Tracking

`char` is a UTF-8 code unit. `string` is a built-in library type backed by `[]char` with UTF-8 encoding.

NRA tracks the **origin** of every string node — `literal`, `allocator`, or `local` (see [§7.1](07-memory-model.md#71-what-nra-tracks) for the complete set):

```zith
// []char implicitly casts to string, zero-cost (literal origin)
let s: string = "hello";

// Concatenation changes origin to allocator -- triggers allocation
let greeting = "hello" + " " + "world";
```

### 3.3 Enum

An enum is a closed set of named compile-time constants. A concrete declared
value type is shared by all constants. Use `enum: union` when named constants
need heterogeneous union members.

#### C-style
```zith
enum Direction { North, South, East, West }
enum Status: i32 { Ok = 0, Err = 1, Pending = 2 }
```

#### Struct-backed
```zith
enum Colors: RGBA {
    red   = { 255, 100, 0, 255 },
    blue  = { 60, 80, 240, 255 },
    green = { 80, 255, 80, 255 },
}
```

### 3.4 Struct

#### Field Declaration & Grouping

Use individual fields for unrelated fields, and `[]` groups for semantically related fields that share a type:

```zith
struct Sample { name: string, age: i32 }

struct Point { [x, y, z]: f32 }

struct Transform {
    [x, y, z]:    f32,   // position
    [rx, ry, rz]: f32,   // rotation
}
```

#### Generic Structs & Self-Referential Patterns

```zith
struct Pair<T, U> { first: T, second: U }

// Illustrative ownership graph. Exact optional-field spelling remains open.
struct Node<T> {
    data: T,
    next: Option<%Self>,
    prev: Option<^Self>,
}
```

#### Implementation Blocks

Structs, enums, and unions can declare methods without bodies in the type definition. The implementation goes in an `implement` block:

```zith
// Struct — declares methods, no body
struct Node<T> {
    data: T,
    next: Option<%Self>,
    prev: Option<^Self>,
    fn isHead(self): bool;   // declared, no body
    fn isTail(self): bool;   // declared, no body
}

// Implementation provides the bodies
implement Node<T> {
    fn isHead(self): bool { self.prev is null }
    fn isTail(self): bool { self.next is null }
}

implement Node<T> as Printable {
    fn print(self) { @println("Node({self.data})"); }
}

// Specific implementation for a concrete type
implement Node<f32> { ... }
```

Components define method bodies inline — they cannot use `implement`:

```zith
component Vec2 {
    [x, y]: f32,
    fn length(self): f32 { sqrt(self.x*self.x + self.y*self.y) }
    fn dot(self, other: Self): f32 {
        self.x*other.x + self.y*other.y
    }
}
```

> Structs, enums, and unions **declare** methods (no body) and **define** them in `implement`. Components **define** methods inline and cannot use `implement`.

#### `self`, `other`, `Self`

`self` is the current instance. `other` is a shorthand for a second instance parameter. `Self` (capitalized) refers to the concrete type currently being implemented.

### 3.5 Component

A plain-old-data (POD) struct, **copy by default** alongside primitives. Components cannot implement traits, and any inline functions are limited to pure transformations.

```zith
component Vec2 {
    [x, y]: f32,
    fn length(self): f32 { sqrt(self.x*self.x + self.y*self.y) }
    fn dot(self, other: Self): f32 { 
        self.x*other.x + self.y*other.y 
    }
}
```

A component must satisfy all of the following constraints:

- Every field is a primitive, another component, or an array/slice of either — no structs, unions, or non-integer enums.
- No trait declarations (`component Name: Traits` is not allowed).
- Inline functions are permitted but restricted to pure transformations:
  - No side effects (I/O, allocation, global mutation).
  - Only arithmetic, comparisons, and field access.
  - Must return a value — `void` is not allowed.
- Copying is always bitwise (memcpy-safe).
- Layout is C-compatible — no vtable, no fat pointers.
- No self-referential ownership or bind fields. See [§7.8](#78-self-referential-types).

### 3.6 Union

Use a `union` for a runtime-tagged value that holds one of several member types.
Its members can be heterogeneous:

```zith
union Numbers {
    i32, f32, u64
}
```

Use a type hint when an expression needs to produce a union:

```zith
enum Flag { A, B, C }
let flag = Flag.A;

let value: union = when (flag) {
    A = 42,
    B = 3.14,
    C = true,
};
```

> Without an explicit `union` type hint, the compiler cannot deduce a union — it must be stated explicitly.
>
> `dyn` works the same way as a type hint — see [§14.2](14-polymorphism.md#142-dyn-as-a-type-hint).

`raw union` is an untagged C-style union, valid only inside `raw` contexts. Accessing the wrong variant is undefined behavior.

Use `enum: union` for a closed set of named compile-time constants whose
values can have different types:

```zith
enum ADT: union {
    One = Point{5, 5, 0},
    Str = "lol",
    F32 = 0.5,
}
```

#### Named Union Variants

A tagged union can name each alternative and associate it with a payload shape:

```zith
union Shape {
    Circle = { radius: f32 },
    Rect   = { w: f32, h: f32 },
    Point,
}

fn area(s: Shape): f32 {
    when (s) {
        Circle = s.radius * s.radius * 3.14f,
        Rect   = s.w * s.h,
        Point  = 0,
    }
}
```

> Both `union` & `enum` can have methods, but neither can implement traits.
>
> Pack literals in `enum:union` variants are treated as anonymous structs with a concrete layout.

### 3.7 Union Narrowing (`is`) & Flow Typing

The `is` operator narrows a union within a branch. Branches are isolated — moves inside one branch don't affect others. After the block completes, the type **widens back** to the full union. The underlying tag does not revert — the value stays `i32` internally — but the type system treats it as the full union again.

When you write a conditional as an expression and not every branch returns a value, the missing branches return `null`. The result becomes `?T`:

```zith
let result = if (v is i32) v;   // ?i32 — missing branch returns null
```

Using the narrowed value outside the `if` is a **compile error**. Recover it by storing the result of the branch expression.

```zith
fn handle(v: Val): void {
    if (v is i32) {
        @println("int: {v}");    // v is i32 here
    } else (v is f64) {
        @println("float: {v}");
    } else {
        @println("str: {v}");    // compiler knows v is []char here
    }
    // v is Val again (full union)
}

// Widening — mutation inside a branch
when (v) {
    i32 = { v = 42 },   // v is still i32 internally, but types as Val after
}
// v is Val again here

// when with branch tags
when (v) {
    n: i32  = @println("int: {n}"),
    f64     = @println("float: {v}"),
    _       = @println("other"),
}

when (shape) {
    Circle = @println("circle r={shape.radius}"),
    Rect   = let [val..] = shape,   // val = w; remaining fields ignored
    _      = @println("other"),
}

// Standalone boolean narrowing & compile-time reflection
// All boolean conditions must be wrapped in parentheses.
let numeric  = (v is i32) or (v is f64);
let isStruct = (T is @struct);
```

### 3.8 Generics

```zith
// Explicit constraints
fn serialize<T: Serializable + Printable, U: Clone>(val: T, ctx: U): string { ... }

// Implicit constraints inferred from usage at the call site
fn add(a, b) { a + b }   // Arithmetic is implicitly required
```

### 3.9 `when` — Pattern Matching

```zith
when (count) {
    0       = @println("none"),
    1       = @println("one"),
    2..=10  = @println("few"),
    _       = @println("many"),
}

// As an expression
let label = when (score) { 
    90..100 = "A",
    70..90 = "B", 
    _ = "C" 
};

// '..' ignores the remaining fields
when (point) {
    [x..] = @println("x=", x),
    _     = @println("no match"),
}

// '..' before a binding captures the last element
when (point) {
    [..w] = @println("w={w}"),
    _     = @println("no match"),
}
```

### 3.10 Cast Operator

```zith
let n: i32 = 42;
let f = n as f64;
```

---

## 4. Traits, Interfaces & Capabilities

### 4.1 Traits vs. Interfaces

| | Trait | Interface |
|---|---|---|
| **Typing** | Nominal. A type explicitly implements a trait. | Structural. A type satisfies an interface automatically when all its conditions hold. |
| **Purpose** | Names behavior and capabilities a type opts into. | States a static contract that a type or value must satisfy. |
| **Composition** | `extends` composes traits. | Bounds combine interfaces with `+`. |
| **Methods** | Requirements and optional default bodies. | Exact-signature requirements without default bodies. |
| **Fields** | Available through an interface required by the trait. | Guarantees field existence and type; qualifiers constrain mutability, while the memory access mode determines whether writes are permitted. |

### 4.2 Traits

Traits are nominal contracts. A type must explicitly implement a trait. Trait
methods may have default bodies. Trait clauses follow the name on separate
lines, in the order `extends` then `requires`. `extends` composes traits.
`requires Interface` requires the trait's `Self` type to satisfy that interface.
The requirement propagates to generic bounds, so `T: Movable` also gives the
body the interface guarantees required by `Movable`. Use `self` / `other` for
value parameters and `Self` for the implementing type.

```zith
interface Positioned {
    [x, y]: i32
}

trait Movable
    requires Positioned
{
    fn moveBy(self, dx: i32, dy: i32) {
        self.x += dx;
        self.y += dy;
    }
}

fn move<T: Movable>(value: T, dx: i32, dy: i32) {
    value.moveBy(dx, dy);
}
```

### 4.3 Interfaces

An interface is a static contract, not a behavior trait or a dynamic-dispatch type. A type satisfies
an interface automatically when its type-level and value-level conditions hold. No `implement`
declaration is needed. Interface conditions combine conjunctively. Repeated identical conditions
count once, while contradictory or incompatible conditions make satisfaction invalid.

```zith
interface SafeNormalize requires @struct {
    [x, y]: i32,
    @ensure(self.y is not 0)
}
```

Header `requires` constraints such as `@struct` are checked when the compiler
evaluates whether a type satisfies the interface.
A field requirement guarantees that the field exists with the declared type.
Code under the interface bound may read that field. Field qualifiers can
constrain mutability, while the memory access mode still controls whether a
write is permitted:

```zith
interface Normalize {
    var [x, y]: i32,
    @ensure(self.y is not 0)
}
```

Unqualified `[x, y]` follows the mutability available through the passed value.
With an `&mut` reference, it requires mutable fields and permits writes through
that access. With an `&` reference, access remains read-only regardless of the field
declaration.
`let [x, y]` promises that the fields are immutable while the contract is
active. `var [x, y]` requires mutable fields; writing still requires a mutable
access mode such as `&mut`. In particular, `&` remains read-only, even for
fields declared `var`; it never permits interior mutation. `var` does not add
implicit synchronization or relax cross-thread safety requirements.

A method requirement names an exact signature. Calls through an interface bound
use static dispatch, and interfaces cannot provide method bodies.

`self` conditions are checked at the call boundary and remain invariants while
the interface contract is active. Writes and calls must preserve them. The
compiler must prove each `@ensure`; if it cannot, compilation fails rather than
inserting a runtime check. `@assume`, `@ensure`, and `@maybe` keep their
existing meanings. In particular, `@assume` is diagnosed when it contradicts
facts already known to the compiler.

Combine contracts and behavior explicitly in a generic bound:

```zith
fn normalize<T: SafeNormalize + Arithmetic>(value: T): T {
    // The interface and trait bounds are both required.
    value
}
```

The full-Zith model does not define `dyn Interface`. Zith-- currently supports method dispatch
through `dyn Interface`; that implementation behavior is documented separately and is not part
of this contract.

### 4.4 Capabilities — Built-in Reference

Capabilities are special traits that feed the compiler more information, unlocking special rules and optimizations.

| Capability | What it does |
|---|---|
| `Copy` | Implicit bitwise copy. Components and primitives are `Copy` by default. |
| `Functor` | `operator()` — makes a type callable like a function. |
| `Arithmetic` | Operators `+`, `-`, `*`, `/`, `%`, and so on. |
| `Error` | Marks a type as an error value. It does not define whether a value is valid or invalid. |
| `Failable` | Defines a type's valid and invalid states. The compiler invokes its state-checking contract for `try`. |
| `Invalid` | A `Failable` type may implement it to expose methods available only after flow analysis proves that its value is invalid. |
| `Allocator` | To provide custom allocators |
| `Generator` | Allows creating runtime-defined resumable or streaming protocols without introducing a dedicated core function kind. |
| `Share` | Capability for values that may cross thread boundaries. It is separate from reference and ownership forms. |
| `ThreadBackend` | Provides a concrete thread handle for explicit `fork`/`merge`, e.g. `pThread` |
| `Lent` | Enables runtime-checked access to immovable global bindings. Also allows `&mut` parameters. |
| `Trust` | A trait extending `Trust` may contain `raw fn` methods callable from safe contexts. |
| `Unique` | Marks a singleton type. It cannot be instantiated — the type name itself acts as the instance. All fields must implement `Share` (thread-safe). A `%Local` variant is a singleton thread-local. |

#### `Failable` & `Invalid` — Invalid-State Capabilities

`Failable` describes a type that can hold either a valid or an invalid state. Its contract
provides `check(): bool` to report whether the value is valid, `valid()` to represent its
valid state, and `invalid()` to represent its invalid state. Users do not call these contract
members directly. For `try x`, the compiler evaluates `x` once, calls `check()` once, then calls
exactly one of `valid()` or `invalid()` based on the check result.

```zith
// Config also implements Failable.
implement Config as Invalid {
    fn onUnavailable(self) { log("Config is unavailable"); }
}
```

`Invalid` is an optional capability for a type that implements `Failable`. It lets the type
provide methods for its invalid state. Those methods are callable only when flow analysis
proves the receiver invalid. For example, `cfg.onUnavailable()` is valid only in a branch
where flow analysis proves `cfg` invalid. An invalid state may be `Nil`, an error value, or a
state defined by the `Failable` type.

`Error` is a marker capability, not a generic error type. The type of an invalid value implements
`Error` to classify that value as an error. `fail` captures only invalid values whose types
implement `Error`, while `catch` can capture any invalid value produced during `with`
initialization. The exact declaration syntax for a marker-only capability implementation remains
open. `Null` and `Fail` are deprecated capability names.

#### `Trust` — Safe Sections with Raw Code

```zith
trait Place
    extends Trust
{
    raw fn sample(): i32 {}
}

fn safeCaller(a: Place) {
    let v = a.sample();
}
```

#### `Unique` — Singleton Types

```zith
struct AppConfig: Unique { host: string, port: u16 }

AppConfig.host = "localhost";
AppConfig.port = 8080;
// let cfg = AppConfig { ... };  -- COMPILE ERROR
```

#### `Share` — Values Across Thread Boundaries

`Share` is a capability for values that may cross thread boundaries. It is
not a reference or ownership form. The older `global: share` spelling appears
in the accepted thread protocol examples below and remains to be reconciled
with the full-Zith reference model.

#### `ThreadBackend` — Thread Fork/Merge

`ThreadBackend` is the runtime side of explicit thread fork/merge. A backend
object such as `pThread` creates a concrete `Thread<T>` handle; `merge` then
blocks and consumes that handle once:

```zith
let t: PThreadHandle<i32> = pThread fork Update(share state, n);
let result: i32 = merge t;
```

The thread examples below retain `share` payload notation from [ADR-0015](adr/0015-full-zith-thread-fork-merge.md).
That notation is part of the accepted thread draft and has not yet been reconciled
with [ADR-0033](adr/0033-nra-reference-model-and-bind.md).

`fork` is a core keyword that names the entry action and the backend object;
`spawn Entry(args)` is a stdlib shorthand for the active backend. There is no
`await`, future, or resumable task in this protocol. The returned value is
exactly the result type declared by the entry action, including failable types
when the action can fail. See [the branch protocol plan](plans/branch-protocol.md)
for the full design.

### 4.5 Operator Overloading

Operator overloading is capability-based and strict — you implement only the operators you need:

```zith
implement Vec3 as Arithmetic {
    fn +(self, other: Self): Self { ... }
    fn -(self, other: Self): Self { ... }
}

implement Pipeline as Functor {
    fn (self, input: []u8): []u8 { ... }
}
let out = pipe(raw_bytes);
```

### 4.6 Trait Composition (`extends`)

`extends` composes one trait into another. Composition includes method requirements and defaults,
and carries capability identity. It does not inherit or embed fields. Structs declare their own
fields directly. A type that implements a composed trait satisfies the traits it composes without
counting their shared capabilities more than once.

```zith
trait Readable {
    fn read(self): i32;
}

trait BufferedReadable
    extends Readable
{
    fn bufferedRead(self): i32 { self.read(); }
}
```

When both clauses are present, write `extends` first and `requires` second,
each on its own line beneath the trait name.

If multiple composition paths reach the same original method or capability, that shared origin
counts once. Methods with the same exact signature are one requirement. Different defaults for
that signature conflict unless the composing trait resolves the conflict with `#[override]`.
That override replaces the inherited default. Its body is optional. Without a body, implementors
must provide the method. An override applies only to an inherited method with the same signature.

Composing a capability and one of its subtraits separately is a conflict. For example, if
`LogError extends Error`, a type must not implement both `Error` and `LogError` as separate
implementations. Implementing only `LogError` includes the `Error` capability once.
---

## 5. Functions

### 5.1 Return Types & Implicit Returns

```zith
fn add(a: i32, b: i32): i32 { a + b }   // explicit type, implicit return
fn add(a: i32, b: i32)      { a + b }   // inferred type

// Bounds-checked indexing: returns the element if in range, otherwise
// propagates null via the implicit optional from '?'.
fn first<T>(slice: []T): ?T {
    slice[0]?
}

fn pick(flag: bool): i32 {
    if (flag) {
        1
    } else {
        2
    }
}
```

> The compiler cannot infer a `union` or `dyn` return type without an explicit type hint.
> Non-void functions may use an implicit return only when every possible path produces a value
> or otherwise terminates. Falling off an `if` without `else`, a `when` without a default, or an
> empty body is a diagnostic.

### 5.2 Function Kinds

| Kind | Description |
|---|---|
| `fn` | Standard runtime function. |
| `const fn` | Compile-time function; parsing is in progress, evaluation is not implemented yet. |
| `state` | Direct state functions; `dock` starts a state machine and `jump target(args)` is a terminating LLVM `tailcc`/`musttail` transition. States in one machine share a return type but may have different parameter lists ([§9.4](09-control-flow.md#94-state-functions-and-state-machines)). |
| `raw fn` | Always unchecked, bypassing NRA and safety checks for C interop. |
| `extern fn` | Fixed C ABI linkage; never name-qualified and never overloaded. |

> The five function kinds are exclusive and cannot be combined: there is no `raw const fn`,
> `extern raw fn`, or similar spelling. `raw fn` and `extern fn` are separate concerns: `raw fn`
> opts out of NRA, while `extern fn` selects the C ABI.

In Zith--, macro calls use the `@` prefix, such as `@println`, `@log`, and `@serialize`. Ordinary function calls use a bare name, such as `console.write`, `process`, or `save`. See [§15](15-macros.md) for the Zith-- rule.

### 5.3 Runtime Tasks, Coroutines, and Concurrency APIs

```zith
// Runtime task types are ordinary library types.
// Thread protocols use fork/merge; Task-style scheduling is stdlib surface.
fn fetch(url: string): Task<Response!> {
    return runtime.schedule(url);
}
```

Concurrency is modeled by `fork`/`merge` plus `stdlib`/runtime APIs, not by a
function kind such as `async fn`. A library may expose `Task<T>`, `Generator<T>`,
channels, executors, or thread handles, but the compiler only sees ordinary
declarations, calls, traits/capabilities, and the NRA facts needed to validate
resource usage around them.

`async`/`await` is out of scope for the current design, and a normal
`async`/`await` model is expected to remain banned. A suspension point would
hide ownership and lifetime effects from the NRA, so asynchronous execution is
left to runtime APIs or `context` surfaces whose contract the compiler can
validate. `state` is the intended alternative, because its suspension and
resumption points are explicit and can reproduce the behaviour of an `async`
when a program needs it.

## 6. Mutability & Bindings

### 6.1 Deep Mutability Model

Zith uses deep mutability: a modifier on a binding flows into every nested field
unless a struct field explicitly overrides it. An unqualified field follows its
owner's content mutability. `let field` keeps that field immutable even when its
owner is mutable. `var field` keeps it mutable through an otherwise immutable
owner.

`var field` does not bypass a read-only memory access. An `&` reference cannot write
the field, even when it is declared `var`. The qualifier also does not add
synchronization or relax cross-thread safety requirements.

```zith
struct Counter {
    value: i32,       // follows the owner's mutability
    let id: u64,      // always immutable
    var scratch: i32, // mutable through a writable access
}

fn update(counter: &mut Counter) {
    counter.scratch += 1;
}

fn inspect(counter: &Counter) {
    // counter.scratch += 1; // COMPILE ERROR: & is read-only
}
```

### 6.2 Binding Keywords

| Keyword | Controls | Semantics |
|---|---|---|
| `let` | Binding | Immutable — cannot be reassigned. |
| `var` | Binding | Mutable — can be reassigned. |
| `global` | Binding | Static storage duration. |
| `const` | Binding | Compile-time constant. |


> `let`/`var` control reassignability of the binding itself. Content mutability is handled separately through memory modifiers ([§7](07-memory-model.md)).

```zith
// let/var control REBIND only. Content mutability comes from memory modifiers.
let x: mut Point;      // cannot reassign x; Point's fields are mutable (mut)
var y: Point;          // can reassign y; Point's fields are immutable (default, no mut)

// References state their access mode.
fn update(p: &mut Point) { p.x += 1; }
fn read(c: &Config) { ... }

const PI = 3.14159;
const COUNT: mut = 0;
COUNT += 1;   // valid at compile time only
```

### 6.3 Destructuring

```zith
let [x, y, z]: f32 = 1.0f;             // grouped same type, related semantics
let name: string; let age: i32; // individual unrelated
let [x,y,z] = | 5,4,'c'|;   // pack literal — see [§6.4](#64-pack-literals)

// If the loop never runs, 'or' supplies the fallback value
let r = for ([acc, i]: i32), (i in 0..n) {
            acc *= i + 1
        } or 0;
```

### 6.4 Pack Literals

Packs group heterogeneous values into a lightweight tuple-like structure. They are declared with `| |` and can be destructured with `[ ]`:

```zith
// Pack literal
let p = | 5, 4, 'c' |;

// Destructure
let [a, b, c] = p;

// Used in for loops with type annotation
let r = for ([acc, i]: i32), (i in 0..n) {
            acc *= i + 1
        } or 0;
```

> Packs are like anonymous structs — the compiler extracts fields by order and passes them as function arguments. They have a concrete layout determined at compile time. They are primarily used for destructuring and as loop accumulators.

---

## 7. Memory Model (NRA)

### 7.1 What NRA Tracks

The ownership system is split conceptually into two layers:

- `NTA` accumulates semantic facts over a representation that still preserves resource identity,
  qualifier distinctions, captures, escapes, branch facts, and return-path structure.
- `NRA` consumes those facts, applies the four ownership rules, emits diagnostics, and performs
  only the internal canonicalizations that are safe to materialize after the proof boundary.

NRA watches every value in your program and classifies it into one of three states:

| State | Meaning |
|---|---|
| `alive` | Ready to read or use |
| `dead` | Moved away — you cannot read it, only reassign |
| `lent` | Temporarily borrowed — exclusive while the borrow lasts |

It also tracks the **origin** of each node — where the value came from:

| Origin | Example |
|---|---|
| `literal` | `"hello"`, `42` — zero cost, no allocation |
| `allocator` | Heap-allocated via `new` or concatenation |
| `local` | Stack variable |
| `reference` | Non-owning access to another node through `&T` or `&mut T` |

With these two axes (state + origin), NRA enforces the rules in [§7.4](#74-the-four-nra-rules).
NTA also records aliasing, branch-local facts, whether a return value is the same node received as
an argument, and whether a bind or reference escapes its legal lifetime.

### 7.2 Move Semantics

Moving `a` to `b` redirects the name `b` to `a`'s node. The name `a` is considered **dead** / **invalid** and cannot be read — only reassigned:

```zith
var a = Point { x: 1.0, y: 2.0 };
let b = a;                          // b -> a's node; a becomes dead
// println(a.x);                    -- COMPILE ERROR: a is dead
@println(b.x);                       // OK

a = Point { x: 3.0, y: 4.0 };      // OK: reassignment creates a new node for a
```

In effect, if `a` is never reassigned, it is as though `a` never existed and `b` has held `Point { x: 1.0, y: 2.0 }` all along.

### 7.3 Memory Modifiers

The accepted full-Zith surface in [ADR-0033](adr/0033-nra-reference-model-and-bind.md)
uses `&` for references, `^` for binds, and `%` for own values. Older spellings
such as `lend`, `view`, `share`, and `belong` are not the canonical forms.

| Form | Relationship | Common use |
|---|---|---|
| `default` | Owned. Lifetime follows the binding. | Variables, struct fields |
| `&T` | Read-only, non-owning reference. It is region-bound and pins its source against relocation or consumption while live. | Reading without taking ownership |
| `&mut T` | Writable reference with the same region and pinning rules as `&T`. | Temporary mutation |
| `^T` | Non-owning bind with no region limit or pin. It is invalidated if its target is consumed or relocated. | General lifetime dependencies |
| `%T` | Own value. It represents logical ownership of an address or slot. | Ownership transfer |

In a type, `&T` and `&mut T` declare reference types. This is distinct from
prefix `&` applied to an expression. The latter remains the Zith-- address-of
form and keeps its documented Zith-- semantics. The accepted full-Zith model
does not convert `&` or `^` references into raw pointers.

`^` expresses a general lifetime dependency, not only a structural parent
relationship. A bind can be a parameter or a return value. A live reference
pins only its referenced subgraph, so a reference to one field does not block
an unrelated sibling field from moving.

### 7.3.1 Re-binding With `:=`

`=` assigns values; `:=` re-binds a reference or bind, or installs an owner
into a slot that is awaiting one.

```zith
var slot: %Buffer = acquireBuffer();  // Initialization uses `=`.
slot := acquireBuffer();              // Error while the first owner is live.
```

`:=` never overwrites a live owner. Rebinding `&` or `^` changes the target
relation and does not create ownership. A bind target must satisfy the normal
lifetime rules.

`%T` is the full-Zith spelling for an own value. A stack-backed owned handle
may not escape its storage scope.

> `%` provides compile-time single-owner guarantees for local bindings. In a
> `global` context, the existing `Lent` capability manages runtime-checked
> distribution. `global` bindings cannot be moved.

> In practice, most code uses `&T` and `&mut T` for references.

#### Implicit Mutability

Reference access mutability is explicit:

| Form | Access | Example |
|---|---|---|
| `&T` | Read-only | `fn read(c: &Config) { ... }` |
| `&mut T` | Writable | `fn update(p: &mut Point) { p.x += 1; }` |
| `default` | Depends on `mut` | `let x: Point;` is immutable; `let x: mut Point;` is mutable. |

The spelling and access mutability of writable bind and own forms are not
fixed by ADR-0033. `default` continues to use `mut` to control content
mutability.

`&T` remains read-only for every field, including a struct field declared
`var`. A field qualifier does not permit interior mutation through a read
reference. References also do not add synchronization or relax cross-thread
safety requirements.

### 7.4 The Four NRA Rules

**Rule 1 — Boundary Access.** At an access boundary, several readers or one
writer may access the same resource. A read and a writer to the same resource
cannot coexist at that boundary. `&mut T` counts as a writer.

**Rule 2 — No Dead Node Access.** A symbol cannot be read while its node is
`dead`.

**Rule 3 — No Escaping Bind.** A bind cannot outlive the target required by
its lifetime dependency. If the target is consumed or relocated, NRA reports
invalidation at the bind's next use.

**Rule 4 — Reference Pinning.** A live `&T` or `&mut T` prevents relocation
or consumption of its source subgraph. The check does not cover unrelated
sibling fields.

For details on how NRA resolves nodes and validates these rules, see
[§7.9](#79-how-nra-resolves-nodes).

### 7.5 NRA in Practice

```zith
// Writable reference.
fn scale(p: &mut Point, factor: f32) { ... }

// Multiple read-only references may coexist.
fn inspect(a: &Point, b: &Point) { ... }

// A bind can express a non-structural lifetime dependency.
fn parentOf(node: &Node): ^Node { ... }
```

### 7.6 Boundary Before HIR

The main NRA proof runs before the final HIR is formed. That boundary exists so the analysis still
sees:

- binding identity and resource graphs;
- the difference between `default`, `&`, `^`, and `%`;
- branch facts, narrowing facts, and return-path equivalence;
- call, capture, and escape structure before lowering erases it.

The final HIR is therefore not the place where ownership is re-proven. It receives a typed,
desugared, NRA-validated view of the program plus only the residual facts that still matter for
lowering, cache serialization, and backend hints.

### 7.7 Residual Facts and Internal Canonicalization

NRA may materialize limited internal canonicalizations after it has proven the ownership contract,
but those rewrites do not change a public signature or observable ABI. For example, forwarding a
proven move internally or removing a temporary introduced only to preserve ownership is valid;
redefining an exported function's calling convention is not.

Residual facts that may survive into HIR include:

- consumed vs. non-consumed value state when lowering depends on it;
- non-null or otherwise narrowed facts that affect control-flow lowering;
- borrow, capture, or escape decisions that codegen and caching must preserve;
- internal calling-convention details only when they stay behind a stable boundary.

LLVM is not the source of truth for ownership. At most it receives hints already decided by NRA,
such as `nonnull`, `noalias`, `readonly`, `nocapture`, or opportunities to remove redundant
temporaries and stores.

### 7.8 Self-Referential Types

A self-referential structure can use an owning edge for `next` and a bind for
`prev`. The target must have a stable address because a bind does not pin it.
If the target is consumed or relocated, NRA reports the bind's invalidation at
its next use. The exact optional-field spelling and initialization syntax for
this pattern remain open.

### 7.9 How NRA Resolves Nodes

> *This section is relevant for tooling authors and compiler contributors.*

Every symbol gets a **resource node** before final HIR lowering. NTA and NRA are lazy in the sense
that they validate nodes when use, view, move, return, capture, or escape facts make the proof
relevant.

#### Node Validation

When you access a node, NRA checks:

1. The node itself is `alive` (not `dead`).
2. Every node in its **dependency vector** — the fields or resources it belongs to or references — is also valid.

If a node is `dead` (say, after a move), NRA records where and why. You get an error pointing right at the violation.

#### Function Evaluation

NRA caches function results. If it has seen a function before, it reuses the cached analysis.
Otherwise, it inspects every return path:

- **Every** path returns one of the function's arguments → caller's node is **not consumed**
  (ownership stays with you).
- **Any** path doesn't return an argument → the result is **consumed**.

Those return facts are preserved into HIR only in residual form. HIR should not have to rediscover
which node a return came from.

#### Branch Isolation (`if` / `else` / `when`)

Each branch runs in isolation. A move inside one branch cannot affect the others. After all branches
complete, NRA applies the side effects of whichever branch actually ran and emits only the merged
facts that lowering still needs.

## 8. Error Handling

Error handling in Zith uses compiler-managed valid and invalid states with return-based control
flow. A function declares its successful return type, and the compiler infers which invalid
states can occur. Absence and error remain distinct: `Nil` is the universal absence state and is
invalid, while types implement the marker capability `Error` to classify their values as errors.

### 8.1 Failable Types

`Failable` is a capability for types with valid and invalid states. Its contract provides
`check(): bool` to report whether the value is valid, `valid()` to represent its valid state,
and `invalid()` to represent its invalid state. Users do not call these contract members
directly. The compiler invokes them when evaluating `try`.

A type that implements `Failable` may also implement `Invalid` to provide methods for its
invalid state. Flow analysis makes those methods available only after proving the value
invalid.

Use `is @ok` to test a `Failable` value. The true branch narrows the value to its valid state;
the `else` branch proves that it is invalid, including `Nil`, values whose types implement
`Error`, and type-defined invalid states.

The compiler always provides `Nil` as the universal absence state. `Nil` is invalid, but it is
not an error. An invalid value is an error when its type implements `Error`. `Optional` and
`Result` may exist in the standard library as ordinary types that implement `Failable`, while an
error value such as `Err` implements `Error`. The declaration syntax for a marker-only
implementation remains open.

The type wrappers `?T` and `T!` are deprecated. In a function return annotation, `T!` remains
valid and means that successful return values have type `T`; the compiler infers the possible
invalid states:

```zith
fn loadConfig(path: string): Config!
```

Postfix `!` propagates the invalid state of an operation to the enclosing function. The
compiler preserves the original invalid value.

> **Zith-- compatibility:** The current compiler still accepts legacy `?T` forms, including
> nullable C pointers, and postfix `?` propagation of `Nil`. That implementation behavior is
> separate from the full-Zith failable model.

### 8.2 `must`, `assert`, and `raw`

`must` guards a failable value and terminates when it is invalid. `assert` checks a boolean
condition. They are separate operations. `raw` extracts a value without checking its state.

```zith
let cfg = must loadConfig(path);
let unchecked = raw cfg;
assert(condition);
```

When `must` terminates, DEBUG mode writes the source location, expression, and invalid-state
category to `stderr`. RELEASE mode does not print this message. Both modes exit with the same
category-specific status code. The numeric codes are not yet specified.

### 8.3 `try`, Local Results, and Fallback

```zith
fn readConfig(path: string): Config! {
    let file = try File.open(path) or defaultFile;
    let data = try file.read() or defaultData;
    parse(data)!
}

let config = try loadPrimary() or loadBackup() or defaultConfig;
```

For `try x`, the compiler evaluates `x` once, calls `x.check()` once, then calls exactly one of
`x.valid()` or `x.invalid()`. Users do not call these contract members directly. The expression
produces a local failable result that can be stored and tested later with `is @ok`:

```zith
let outcome = try loadConfig(path);
if (outcome is @ok) {
    use(outcome);
} else {
    // The invalid result remains local and can still be tested here.
}
```

An invalid result is not automatically propagated out of the enclosing function.

`or` evaluates its fallback only when the preceding result is invalid. It handles any invalid
state, including `Nil`, errors, and invalid states from user-defined `Failable` types. If the
left side is valid, its valid value is the result and the fallback is not evaluated. If every
alternative is invalid, the expression retains the last alternative's invalid state as its
local result. The current invalid state is not passed to the fallback.

Postfix `!` propagates the invalid state of an operation to the enclosing function. It does
not convert `Nil` into an error or discard the original invalid value.

### 8.4 `with` / `catch`

`with` evaluates its initialization expressions in order. If one produces an invalid value,
evaluation stops, the body is skipped, and the attached `catch` receives that original invalid
value. `catch` handles any invalid state, including `Nil` and states from user-defined
`Failable` types. It does not require the value's type to implement `Error`.

The attached `catch` handles initialization only. It does not handle invalid states produced by
operations in the `with` body. Use `fail` in the body to capture error values there. The block
shape below illustrates the current proposal; exact handler grammar remains under discussion.

```zith
with [connection: connectDb(), user: getUser(connection)] {
    process(user);
} catch (invalid) {
    // invalid is the original value produced during initialization
}
```

### 8.5 `fail` Blocks

A `fail` block listens for invalid outcomes in its lexical scope after the block is declared.
It captures an invalid value only when that value's type implements the marker capability
`Error`. It does not capture `Nil` or other invalid values whose types do not implement `Error`.
The block receives the original value with its original type, not a generic `Error` value or a
formatted diagnostic.

```zith
{
    fail (err) {
        if (err is NotFound) {
            resume defaultConfig;
        }
    }
    let config = loadConfig()!;
}
```

`resume x;` replaces the failed operation's result with `x` and continues after that operation.
The replacement must match the operation's successful result type. If the error is not
resumed, it continues propagating. `fail` is distinct from both `or` and `catch`: `or` handles
any invalid state without passing it to the fallback, `catch` receives any invalid value from
`with` initialization, and `fail` receives only invalid values whose types implement `Error`.

### 8.6 `throw`

```zith
fn divide(a: i32, b: i32): i32! {
    if (b == 0) throw DivisionByZero;
    a / b
}
```

---

## 9. Control Flow

### 9.1 Syntax Rules

Parentheses `()` are mandatory on every control structure's condition except function calls. Logical operators use English keywords; bitwise operators use standard symbols followed by `.`:

```zith
if (x > 0 and y < 10) { ... }
if isTrue() and (x > 5) { ... }
let mask = a &. b |. c ^. d;
```

### 9.2 `for`

```zith
for { ... }                                     // infinite
for (i in 0..=9) { @println(i); }               // inclusive range
for (i in 0..9)  { @println(i); }               // exclusive range
for (i = 0), (i < 10), (i += 1) { ... }         // init / cond / step
for (v in range(0, 100)) { @println(v); }       // over a generator

// Destructured group with fallback
let r = for ([acc, i]: i32), (i in 0..n) { acc *= i + 1 } or 0;
```

> If the loop body may never run, its return value may be invalid. `or` can provide a fallback.

The current Zith-- compiler accepts an iterator protocol whose `next(self)` returns legacy
`?T`, with `null` ending iteration. The full-Zith iterator result protocol has not yet been
specified.

> The init/cond/step form accepts comma-separated, parenthesized expressions — `for (i = 0), (i < 10), (i += 1)` — or the flat alternative, `for (i = 0, i < 10, i += 1)`.

### 9.3 Pipeline (`|>` and `do`)

Pipelines are explicitly threaded. Each stage reads the current value through
`..`; there is no automatic injection of the value as an argument, no tag
capture, and no `?`/`!` propagation out of the chain. `x |> f(..)` is
equivalent to `f(x)` except that the source is materialized once before the
stage runs. A `|>` stage must reference the current value exactly once.

`do` is a side-effect stage: it may use `..` zero or one times, keeps the chain
value unchanged, and is valid only as a chain operator. `..` is valid only
inside a pipeline stage.

`|>` replaces the chain value with the stage result. `do` runs a side-effect
stage and keeps the chain value unchanged, so `x do f(..)` returns `x`. `do` is
not a standalone statement. Both operators are left-associative and have lower
precedence than function calls.

```zith
getData() |> process(..) |> save(..);

getData()
    |> parse(..)
    |> validate(..)
    |> save(..);

// Inline side-effect: observe the current value without advancing the chain.
readFile("data.bin")
    do log(..)
    |> process_body(..);

// A block effect stage can bind locals, but must not transfer control.
readFile("data.bin")
    do { let h = parse_header(..); validate(h); }
    |> process_body(..);
```

### 9.4 `state` Functions & State Machines

A `state` function is a normal function that belongs to a state machine. `dock` starts a state
machine and returns its eventual `state` return value; `jump` is a terminating transfer to
another state in the same machine. The compiler emits LLVM `musttail tailcc` calls for direct
state-to-state transitions and rejects old `flow`/`marker` spellings.

- **`state`**: `state Name(params): ReturnType { ... }` declares one state in a machine.
  All states in the same machine share the same return type; their parameter lists may differ.
  LLVM `tailcc` lets a `jump` between different signatures transfer without growing the call
  stack or introducing a hidden context frame.
- **`dock`**: `dock Start(args)` is an expression that calls a state and evaluates to the
  value returned by the last state in the machine. It may be assigned to a binding and
  returned by an ordinary function.
- **`jump`**: `jump Next(args);` terminates the current state and tail-calls the named state
  from the same machine. It is only valid inside a `state` body, validates against the target
  state's own parameter list, and lowers to `musttail tailcc` followed by `ret`.
- **`return`**: `return value;` inside a state returns from the whole state machine and gives
  the `dock` call its result. State functions are ordinary LLVM functions, and each transition
  is a direct `musttail` call immediately followed by `ret`.
- **`state(params): ReturnType` value**: a real state declaration can be stored in a value
  with the matching `state(params): ReturnType` signature. `dock` accepts that value with the
  same argument and return-type checks as a direct state call, and the call is emitted as an
  indirect `tailcc` call.

```zith
state Count(n: i32): i32 {
    if (n == 0) {
        return n;
    }
    jump Count(n - 1);
}

fn main(): i32 {
    let result = dock Count(3);
    return result;
}
```

```zith
state Machine(n: i32): i32 {
    return n;
}

fn main(): i32 {
    let S: state(i32): i32 = Machine;
    return dock S(42);
}
```

States can read module/global state and declare ordinary local bindings. The restrictions are
that every state in a machine must share the machine return type, `jump` must be the last
statement of a state block because it is a terminating transition, and `jump` arguments must
match the target state's own arity and parameter types.

#### State Machine Rules

| Rule | Detail |
|---|---|
| Return type | **Required.** Every `state` in the same machine has the same return type. |
| Parameters | **Diverging allowed.** States may use different parameter lists; `jump` and `dock` validate against the specific target state. |
| Entry | **Working.** `dock State(args)` reuses normal call argument coercion and returns the machine return type. |
| Transition | **Working.** `jump Next(args)` resolves a direct state symbol and lowers to a `musttail` call followed by `ret`. |
| Stack use | **Working.** Declarations and calls use LLVM `tailcc`; transitions compile to `musttail tailcc` calls, so recursive state loops and diverging transitions do not grow the stack on supported targets. |
| Scope | **Working.** State bodies are ordinary function bodies; they can use module/global state and local bindings. |
| Legacy syntax | **Rejected.** `flow fn`, `marker`, `stackful marker`, and old `dock { ... }` blocks are removed. |

State machines use `musttail tailcc`, so support is limited to targets that preserve tail-call
guarantees in the backend. Diverging parameters do not create an implicit context, frame, or
runtime blob. The compiler emits an unsupported-target diagnostic instead of claiming
arbitrary-target support.

---

## 10. Concurrency & Runtime APIs

### 10.1 Core-Language Position

Zith's core language defines `fork`/`merge` as the explicit thread statements,
but does not define `async`, coroutines, schedulers, or function kinds for
concurrency. There are no dedicated HIR nodes for `await` or coroutine
suspension. The compiler understands only:

- ordinary declarations and calls;
- the `fork` and `merge` thread protocol;
- library-defined handle, channel, task, or executor types;
- traits/capabilities used to describe what those types guarantee;
- NRA facts about sharing, lending, capture, escape, and ownership across those calls.

### 10.2 Runtime Surface

The standard library or an alternate runtime may expose APIs such as thread spawners, executors,
message queues, join handles, or resumable tasks. `spawn` is a stdlib shorthand
for an implicit fork and is activated through a context; the core protocol itself
is explicit:

The following examples preserve `share` payload notation from accepted
[ADR-0015](adr/0015-full-zith-thread-fork-merge.md). Its replacement under
[ADR-0033](adr/0033-nra-reference-model-and-bind.md) remains unresolved.

```zith
use threading.pthread;

let handle = pThread fork Worker(share state);
let result = merge handle;

let shorthand = spawn Worker(share state);   // active backend
let out = merge shorthand;
```

API names above are illustrative. The compiler does not reserve scheduling
helpers; `fork`, `merge`, and the `Thread<T>` handle contract are the stable
language surface. See [10.3](10-concurrency.md#103-thread-forkmerge) for the
full-thread example.

### 10.3 Thread Fork/Merge

The explicit thread protocol uses `fork`/`merge` as core keywords, with runtime
backends as ordinary objects. There are no coroutines, `await`, or implicit
schedulers in the core language:

```zith
let t: PThreadHandle<i32> = pThread fork Worker(share state, n);
let result: i32 = merge t;
```

`fork` hands an entry action to a backend object and returns the backend's
concrete handle (`Thread<T>` minimum). `merge` blocks, consumes the handle once,
and returns exactly the result type declared by the entry. `spawn Entry(args)`
is a stdlib shorthand that uses the active thread backend; it is not a core
keyword. NRA tracks the fork as an ownership transition and rejects unbalanced
forks.

`merge h1 and h2` waits for both threads, consumes both handles, and returns a
tuple of results in operand order. `merge h1 or h2` also waits for both and
consumes both handles, but returns a tagged union containing the result from
the thread that completed first. If threads finish simultaneously, the
leftmost handle wins. The union covers every handle's declared result type,
including failable states, and need not distinguish handles that return the
same type. The `or` form does not cancel or skip the other thread.

`revoke x;` removes access to `x` from every unbounded thread in the statement's
scope that holds revocable access to it. `revoke h1;` revokes all revocable
resources passed to the thread represented by handle `h1`. Both forms prevent
new guarded accesses and wait for active guarded operations to finish. Neither
form destroys resources or terminates a thread, and neither consumes the
handle.

### 10.4 What the Compiler Proves

Concurrency-related safety is enforced through the same pre-HIR ownership proof used everywhere
else:

- whether a call duplicates a resource illegally;
- whether a borrowed value or bind escapes;
- whether narrowing facts or branch facts justify later lowering decisions;
- whether shared/runtime-managed resources are passed only through the capabilities and wrapper types
  that define the contract.

The compiler does not special-case threads or async control flow. If a runtime API needs stronger
guarantees, it must express them through normal signatures, types, and traits that NRA can reason
about before HIR is finalized.

---

## 11. Comptime

Comptime covers compile-time computation: reflection, type manipulation, and `const` blocks.

### 11.1 Comptime Bindings


```zith
const counter: mut = 0;
counter += 1;   // valid at compile time
// counter += input().nextI32();   // COMPILE ERROR: frozen at runtime
```

### 11.2 `const` Blocks

A `const { ... }` block executes its contents at compile time. Every value inside must be computable at compile time — if anything depends on runtime input, the compiler reports an error.

`const fn` declarations are the future syntax for functions that resolve at compile time. Once
evaluation lands, they would be required to be called only inside a `const` block or assigned to a
`const` binding. Today the compiler only parses the declaration; evaluation is not implemented.

```zith
const result {
    let x = 10;
    let y = 20;
    x + y   // evaluated at compile time
};

import assets/data.json as Data;
const fn processJson(data: []char): JsonValue { ... }
const parsed = processJson(Data);  // runs at compile time
```

Compile-time functions can raise `throw` to halt compilation and display an error message, similar to `static_assert` in other languages.

### 11.3 Reflection

Use `@` intrinsics to inspect types at compile time:

```zith
// Iterate the fields of a struct
for ( field in @fields MyStruct ) {
    @println("{}: {}", field.name, field.type);
}

// Check a type's kind
let isPrim = (T is @primitive);    // bool, i32, f64, etc.
let isStr  = (T is @struct);       // struct
let isComp = (T is @component);    // component
let isUn   = (T is @union);        // union
let isEn   = (T is @enum);         // enum

// Inspect field visibility
for ( field in @fields MyStruct ) {
    @println("{}: {}", field.name, field.visibility);  // pub, mod, private
}

// Check nullability
let nullable = (T is @nullable);   // ?T
```

### 11.4 Type Manipulation

> *This section is relevant for tooling authors and compiler contributors.*

You can create a type and modify it before it is "finalized":

```zith
// Create a new type
type Custom = @struct;

// Add fields -- allowed while the type is not yet returned or instantiated
@appendField Custom, x: i32;
@appendField Custom, y: f32;

// Remove a field
@removeField Custom, x;

// Add methods
@appendMethod Custom, fn distance(self): f32 { sqrt(self.x*self.x + self.y*self.y) }

// The type is "done" once it's returned or instantiated
let p: Custom = Custom { x: 1, y: 2.0 };

// Primitive aliases are IMMUTABLE -- they have no fields to modify
type Celsius = i32;
@appendField Celsius, x: i32;  // COMPILE ERROR: type is 'done' (primitive)
```

> A type built via `@struct` is "done" the moment it is returned or instantiated. Until then, `@appendField`, `@removeField`, and `@appendMethod` are available. Passing the type to a generic function also counts as "done."
>
> A type created with `type` (e.g. `type Celsius = i32`) is a primitive alias — it has no fields to modify and is always immutable. You can still add methods via `implement`, but you cannot `@appendField` or `@removeField`.

---

## 12. Assets

Assets are external files — JSON, images, other data — that the compiler validates and makes available at compile time.

### 12.1 Configuration

Declare asset paths in `ZithProject.toml`:

```toml
[project]
name = "my_game"
version = "0.1.0"

[assets]
assets = ["assets/", "../someOtherFolder"]
```

The compiler verifies that every declared path exists and is readable.

### 12.2 Importing Assets

```zith
import assets/data.json as Data;
import assets/sprites/player.png as PlayerSprite;
```

> `as` is mandatory here, to avoid conflicts between files that share a name but differ in extension.

Imported assets are available as compile-time constants.

### 12.3 Processing Assets at Compile Time

Combine assets with `const fn` to process them before the program runs:

```zith
import assets/config.json as ConfigData;

const fn parseConfig(data: []char): Config {
    // parse JSON at compile time
    JSON.parse(data)!
}

const CONFIG = parseConfig(ConfigData);   // runs at compile time
```

### 12.4 Runtime Assets

Assets not declared in `[assets]` are loaded at runtime with standard file I/O:

```zith
let runtimeData = fs.read("runtime/save.json")!;
```

---

## 13. Raw & Unsafe

### 13.1 Safety Hierarchy

```
Safe code (default)
    └─ needs raw blocks to call raw fn
         └─ needs to be inside a raw block / fn to use unsafe block
```

| | `raw` | `unsafe` |
|---|---|---|
| **What it is** | A base-level bypass, always unchecked. | A stronger bypass, valid only inside `raw` contexts. |
| **Scope** | Any expression or statement. | Only inside a `raw fn` or `raw` block. |
| **Debug mode** | Unchecked. | Unchecked. |
| **Release mode** | Unchecked; compiler warns. | Unchecked; compiler warns. |
| **Use case** | C interop, performance-critical paths. | Operations undefined if misused — pointer arithmetic, inline assembly. |

### 13.2 `raw fn`

A `raw fn` bypasses safety checks in both debug and release. The compiler warns if `raw` could be removed in release builds.

```zith
raw fn c_compat(x: raw opaque): raw opaque {
    // basic validations still exist but allows:
    // mut *, use raw opaque / union & etc..
}
```

### 13.3 `unsafe`

```zith
raw fn dangerous(x: opaque) {
    unsafe {
        // extension of raw — completely disables compiler checks
        // allows: bypass mutability, bit_cast, arbitrary address assignment,
        // inline assembly, etc.
        asm {
            mov rax, x         // x is a Zith var — compiler maps it
            call someFn         // can call Zith fns from asm
        }
    }
}
```

### 13.4 `Trust` as a Bridge

`Trust` bridges safe code to raw and unsafe code. A trait extending `Trust` may contain `raw fn` methods that are callable from safe contexts:

```zith
trait Place
    extends Trust
{
    raw fn sample(): i32 {}
}

fn safe_caller(a: impl Place) {
    let v = a.sample();   // allowed: Trust is in scope via Place
}
```

### 13.5 When to Use Each

| Situation | Use |
|---|---|
| C interop, headerless libraries | `raw fn` |
| Pointer arithmetic, inline assembly | `unsafe` inside `raw fn` |
| Exposing low-level operations to safe code | `Trust` capability |
| Normal application code | Neither — stay safe |

---

## 14. Runtime: Polymorphism & Dynamic Behaviour

### 14.1 Static vs Dynamic Dispatch

By default, Zith uses static dispatch — the compiler knows the exact implementation at compile time. Zero overhead.

Use `dyn` for dynamic dispatch. At the call site you get polymorphism; the compiler and LLVM can often optimize away the indirection, making it zero-cost in practice.

### 14.2 `dyn` as a Type Hint

Like `union` (see [§3.6](03-type-system.md#36-union)), `dyn` works as a **type hint**. When the compiler can't deduce the concrete type, you write `dyn` to tell it you want dynamic behavior:

```zith
let x: dyn = 5;
if (x is i32) x += 32;   // smart-cast inside the branch

// dyn []T — dynamic Trait slice
let items: dyn []Drawable = shapes;
// dyn slice
let dynList: dyn [];

// In many cases LLVM strips the dyn overhead entirely
// even if not, compare the type is a simple int comparison
// in terms of performance, is union + ptr indirection, still fast
```

Inside a smart-cast branch (`is`), the type narrows to the concrete type. Mutations inside the branch affect the inner value; outside, assigning to the variable changes what the `dyn` points to (if `var`).

### 14.3 `dyn` Traits

`dyn Trait` is a read-only, non-owning reference with a vtable by default.

The reference and ownership forms apply to `dyn` values:

| Keyword | `dyn` behavior |
|---|---|
| `&dyn Trait` | Read-only dynamic reference. This is the default form. |
| `&mut dyn Trait` | Writable dynamic reference. |
| `%dyn Trait` | Owned dynamic value. |

```zith
fn draw_all(items: dyn []Drawable) {
    for (item in items) { item.render(); }
}

//specific verbose, you could use an interface or alias to simplify
fn modify(shape: &mut dyn Drawable) {
    shape.scale(2.0);
}
```

Full Zith defines `dyn Trait`, not `dyn Interface`. An interface is a static contract and does not
define dynamic dispatch. Zith-- currently supports method dispatch through `dyn Interface`, as
described in [§4.3](04-traits-interfaces.md#43-interfaces) and the implementation-status docs.

When you write a type that could be `dyn` or `opaque`, prefer `dyn` — it's short and clearer. Reserve `opaque` for cases where you specifically need `raw opaque` (untagged `void*`, C interop).

### 14.4 `dyn` vs Generics

| | Generics | `dyn` |
|---|---|---|
| Dispatch | Static — one copy per type | Dynamic — single code path |
| Code size | Larger (N copies) | Smaller (one copy) |
| Performance | No indirection | Vtable indirection (often elided by LLVM) |
| When to use | Hot loops, known types at compile time | Heterogeneous collections, plugins |

```zith
// Generic — compiler monomorphizes
fn log<T: Printable>(val: T) { val.print(); }

// dyn — vtable dispatch, LLVM may inline away the overhead
fn logDyn(val: dyn Printable) { val.print(); }
```

### 14.5 Object Safety

A trait is object-safe if all its methods meet these rules:

- No `Self` in parameter or return types (except `self`, `other`)
- No generic type parameters on the method
- No `Self: Sized` requirements

If you try to use a non-object-safe trait with `dyn`, the compiler rejects it.

---

## 15. Macros

| Type | Description |
|---|---|
| Normal (scoped) | Hygienic for bindings introduced by the macro, but template names are resolved from the call-site scope, so globals and imports remain visible when not shadowed. Requires the `@` prefix at the call site. |
| Raw macro | Inserts code literally at the call site; not hygienic. Names resolve in the call-site scope first and fall back to globals/imports. Also requires the `@` prefix. |

> **Full-Zith distinction:** tags are not macros. `@<` is a dedicated tag-opening delimiter, not
> an intrinsic call or an `@` prefix applied to `<`. The closing delimiter is `</`. Standalone
> `@` remains the marker for compiler intrinsics and compiler magic. See
> [ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

> Contexts are reserved for domain-specific syntax integration, not as general-purpose
> declaration containers ([§17](17-contexts.md)).

> **Zith-- distinction:** normal `macro` and `raw macro` are the only macro forms in Zith--, the
> subset compiled by `main`. Full-Zith tags are separate and do not change the Zith-- contract.
> See [Zith--](Zith--.md).

```zith
macro log(msg: expr) { @println("[LOG] ", msg); }

raw macro swap(a: identifier, b: identifier) {
    let _tmp = a; a = b; b = _tmp;
}

// Default/raw macro with capture attribute
@closure[capture](){ ... }

// Zith-- macro parameter meta-types: identifier, expr, condition, body
```

### Scope and Hygiene

Normal macros keep macro-local bindings hygienic: a `let`/`var` introduced by
the template does not leak into the call site, and a call-site local does not
accidentally capture a same-named macro-local. Names that are not macro-locals
(e.g. a function or global referenced by the template) still resolve through
the call-site scope chain and can reach globals and imports.

Raw macros are literal: their `let`/`var` bindings and name reads use the
call-site scope. A raw macro name first resolves against call-site locals, then
the enclosing scopes, then the module/global scope. Splice statements remain in
the call-site block and can see names declared before or after the call in that
block.

The `::` scope-resolution operator remains a separate roadmap item; this
chapter describes only the default and raw macro resolution behaviour.

### 15.1 Zith-- `@`-Prefixed Macro Calls

In Zith--, the `@` prefix distinguishes a macro call from an ordinary function
call:

```zith
// Macro call -- @ prefix
@println("hello");
@log("debug message");
@serialize(obj);

// Function call -- bare name
console.write("hello");
process(data);
save(file);
```

This rule describes Zith-- macro calls only. Full Zith uses standalone `@` for
compiler intrinsics and compiler magic. Its `@<` tag delimiter is a separate
compound delimiter.

### 15.2 Full-Zith Tags

Tags provide domain-specific syntax integration in full Zith. They are not
macros, and their bodies are not implicitly treated as Zith statements. A tag
declaration specifies the representation of its required body argument.

```zith
@<p>Se e louco, Zith full e foda</p>
```

The `@<` opener and `</` closer are dedicated delimiters. A tag declaration
must accept a body argument. Its body kind determines how the compiler presents
the content to the tag:

| Body kind | Contract |
|---|---|
| `tokens` | Exact source text as written, without Zith interpretation. |
| `identifier` | Exactly one identifier. |
| `ast` | Structured syntax, not necessarily evaluated. |
| `block` | Code parsed as a Zith block. |

These body kinds are not exhaustive. The exact tag declaration grammar,
attribute grammar, and empty-body rules remain under design. This is a full-Zith
design decision only. Zith-- behavior and its existing `macro` forms are
unchanged.

---

## 16. Words (Custom Operators)

Words let you define custom operators from identifiers. Each word has a fixed position — **prefix**, **infix**, or **suffix** — with language-defined precedence.

- You must activate a word with `use`, even if you already imported its module.
- Two words with the same name in the same scope: compile error.
- If the compiler sees any ambiguity (even potential), it errors out.
- Use a context for words when they participate in a domain-specific syntax integration.
  Contexts are not generic namespaces for public APIs ([§17](17-contexts.md)).

### 16.1 Word Types

| Type | Description | Example |
|---|---|---|
| `operator` | Overload a specific operator (`+`, `-`, `*`, `()`, etc.) | `implement Vec3 as Arithmetic { fn +(self, other: Self): Self { ... } }` |
| `token` | A word with low precedence that does nothing alone. Serves as a syntactic component in domain expressions. | `token SELECT;` |

#### Operator Words

Operator words overload built-in operators. Use `implement` with a capability to define the behavior:

```zith
implement Vec3 as Arithmetic {
    fn +(self, other: Self): Self { ... }
    fn -(self, other: Self): Self { ... }
    fn *(self, scalar: f32): Self { ... }
}

// Custom word — not overloading a built-in operator
use math.vec.dot as DOT;
use math.vec.cross as CROSS;

// Infix — reads as dot(vec1, vec2)
let d = vec1 DOT vec2;

// Prefix — reads as VALIDATE input
let result = VALIDATE data;

// Suffix — reads as input CHECK
let value = input CHECK;
```

| Position | Reads as |
|---|---|
| Infix | `a DOT b` → `dot(a, b)` |
| Prefix | `not x` → `not(x)` |
| Suffix | `x!` → `assert(x)` |

#### Token Words

Token words have low precedence and do nothing alone. They let a domain syntax define low-precedence terms, such as SQL keywords:

```zith
token SELECT;
token FROM;
token WHERE;

// Operator* defines behavior for a token
operator* (SELECT, list) { ... }
```

> Tokens are useful for DSLs where keywords need to be passed as arguments without function call syntax.

### 16.2 Zith-- Macro Compatibility

Macros are available in the Zith-- subset, not in full Zith. The following
distinction describes that subset only:

- **Zith-- macros:** Use call syntax and provide syntax-template expansion.
- **Words:** Work as keywords and can return values.

---

## 17. Contexts

> **Full-Zith design direction:** contexts are reserved for syntax integration with a domain-facing
> API. A context is not an ordinary public API surface or a general-purpose container for
> declarations. See [ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

A context provides an optional syntax integration for a domain such as Math, SQL, or HTML. A
domain API may offer one when it deliberately participates in that domain's syntax. The current
draft allows scoped or global activation, with only one context active at a time in a scope.

```zith
// Illustrative syntax: SQL-specific forms are active inside this block
use SQL {
    SELECT * FROM users WHERE id = :id
}

// Global activation is also present in the current draft
use SQL;
```

### Best Practice

Use a context when an API deliberately integrates with domain-specific syntax. Do not use one
merely to group ordinary public declarations or to create a generic library namespace. Exact
declaration, activation, and distribution rules remain open design questions.

---

## 18. C Interop

Zith offers three modes of C interop, designed to make using existing C libraries as frictionless as possible.

### 18.1 Automatic Binding via `.h`

Including a C header automatically generates bindings. Every function becomes a `raw fn` by default, and pointer types are inferred:

```zith
import "openssl/ssl.h";

// All C functions are now available as raw fn
SSL_CTX_new(method);
```

| C type | Inferred Zith type |
|---|---|
| `T*` | `mut *T` (mutable pointer) |
| `const T*` | `*T` (read-only pointer) |
| `T**` | `mut *mut *T` |
| `void*` | `raw opaque` |
| `int`, `float`, etc. | direct primitive equivalents |

### 18.2 Manual Binding with Semantic Annotation

Override or supplement auto-generated bindings to attach Zith-specific semantics:

```zith
// Equivalent declarations — malloc is a C function (no namespace)
// bindToC is subject to Zith namespace rules
fn bindToC = extern 'C' malloc(size: u64): %opaque;
extern 'C' malloc(size: u64): %opaque;   // same thing, no namespace alias
```

### 18.3 External (No Header)

For assembly routines, headerless libraries, or code deliberately outside the project:

```zith
// The linker resolves this; the compiler has no information about the function
fn bindTo = extern someAsmRoutine(x: u64): u64;
```

### 18.4 Exposing Zith to C

```zith
extern 'C' fn my_function(x: i32): i32 {
    x * 2
}
// Generates a C-compatible symbol, callable from C as an ordinary function
```

---

## 19. Project Configuration

### 19.1 `ZithProject.toml` (per-project)

```toml
[project]
name    = "my_app"
version = "0.1.0"

[build]
runtime = true            # default: true; set false for OS/embedded targets
asm     = "x86_64_intel"  # required if using inline assembly
                          # errors if it diverges from the host machine or other project files

[assets]
paths = ["assets/"]       # compile-time-validated asset paths

[dependencies]
std = "bundled"
```

### 19.2 `ZithFlags` (compiler / global)

```
--runtime=false     # disable the runtime globally: removes most allocators,
                    # disables `must`, disables dynamically linked libraries and
                    # anything that depends on them, and forces all available
                    # std to be statically linked
--asm=arm64         # set the assembly dialect globally
--release           # release mode 
--debug             # debug mode (default)
```

> `runtime = false` disables the heap, standard stack assumptions, and signal handlers. Any standard library feature that requires a runtime becomes unavailable at compile time.

### 19.3 Project Language Identity

> **Full-Zith design direction, not implemented:** a project identity will let a team enable or
> disable optional language features for its own codebase and attach a reason to disabled
> features. It does not redefine the universal public-API style or apply its choices to
> dependencies. The configuration syntax and diagnostic presentation remain open. See
> [ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

---

## 20. Standard Library

### 20.1 Three-Part Structure

| Namespace | Stability | Use when |
|---|---|---|
| `std` | Stable, backward-compatible | You need a guaranteed API |
| `soon` | Experimental, may change | You're prototyping and don't mind breakage |
| `c` | Supported C/runtime surface | You need common low-level or system APIs |

```zith
import std;
import soon;   // use with caution — API may shift
import c;       // supported C/runtime surface
```

The `c` namespace is a compiler/runtime contract, not an implicit host-header
include. Common declarations under `c/...` remain source-compatible while the
target selects their implementation through libc, VM intrinsics, or WASM host
imports. Use `import "file.h"` for platform-specific declarations and external
C libraries that are not part of the supported surface.

### 20.2 Core Modules

#### `std/io/console`
```zith
fn println(msg: []char): void;
fn print(msg: []char): void;
fn eprint(msg: []char): void;
```

```zith
@println("hello");
```

#### `std/collections/DynArray`

```zith
struct DynArray<T> {
    fn push(self: &mut Self, val: T);
    fn pop(self): ?T;
    fn len(self): u64;
    fn get(self, index: u64): ?T;
}
```

#### `std/fs`
```zith
struct File { ... }

fn open(path: string): File!;
fn read(self: &File): []u8!;
fn write(self: &mut File, data: []u8): void!;
```

### 20.3 Common Traits

| Trait | What it enables |
|---|---|
| `Copy` | Bitwise copy — primitives and components get this by default |
| `Clone` | `fn clone(self): Self!` |
| `Lent` | Can appear with a writable reference (`&mut`) parameter |
| `Share` | Safe to share across threads |

---

## 21. Best Practices & Patterns

### 21.1 Ownership Patterns

- **Use `%T` for owned resources:** `let resource: %Resource = Resource.new();`
- **Use `&T` for read access:** `fn process(config: &Config) { ... }`
- **Use `&mut T` for writable access:** `fn update(state: &mut GameState) { ... }`
- **Use `^T` for a non-owning lifetime dependency:** the target must remain stable and valid.
- **Use the `Share` capability for values that may cross thread boundaries.**

### 21.2 Invalid-State Patterns

- **Prefer `try ... or` for any invalid state:** `let config = try loadPrimary() or loadBackup() or defaultConfig();`
- **Keep a `try` result when you need to inspect it later:** bind it to a local and test it with `is @ok`.
- **Keep absence distinct from errors:** `Nil` is invalid, but does not implement `Error`.
- **Use `fail` to inspect an error value:** `fail (err) { log(err); }`
- **Reserve `must` for cases where invalidity is fatal:** `const API_KEY = must env("API_KEY");`

### 21.3 Context Patterns

- Reserve contexts for APIs that deliberately integrate with domain-specific syntax, such as
  Math, SQL, or HTML.
- Do not use contexts as generic namespaces or containers for ordinary public APIs.

### 21.4 Error Handling Patterns

- Use `or` for fallbacks across any invalid state. It evaluates a fallback only after invalidity
  and retains the last invalid result if every alternative is invalid.
- Bind a `try` result when later code needs to test its state with `is @ok`.
- Use `catch` only for invalid values produced during `with` initialization.
- Use `fail` only for invalid values whose types implement `Error`, and use `resume value;`
  to continue with a replacement result.

### 21.5 Context and Tag Patterns

- Reserve contexts for APIs that deliberately integrate with domain-specific syntax.
- Declare a tag's body kind to match the syntax the domain API needs to consume.

### 21.6 Rule of Three

If a function needs more than three specialized tools (state machines, words, contexts, tags, comptime, inline error handling), something went wrong. Split the function or reconsider your abstraction.

```
// Good — two tools: state machine + word
state Init() {
    jump Ready();
}
fn process() {
    dock Init();
    step1 -> step2
}

// Warning sign — four tools in one function
fn process() {
    dock spinning();           // state machine
    use Math;                  // context
    use assert AS CHECK;       // word
    risky()!                   // inline error handling
    // Prefer: move the context/word usage to a wrapper function
}
```

The Rule of Three keeps code readable. Zith gives you many tools — you don't have to use them all at once.

### 21.7 Naming Conventions

| Construct | Convention | Examples |
|---|---|---|
| Variables & functions | camelCase | `playerHealth`, `getDamage`, `loadConfig` |
| Components | single word, lowercase | `rgb`, `color`, `file`, `vertex`, `health` |
| Structs | PascalCase | `Point`, `Container`, `DynArray`, `GameConfig` |
| Traits & interfaces | PascalCase | `Printable`, `iPositioned` (interfaces use lowercase `i` prefix) |
| Files | kebab-case | `game-loop.zith`, `asset-manager.zith` |
| Constants & comptime | UPPER_SNAKE_CASE | `MAX_SIZE`, `PI`, `DEFAULT_TIMEOUT` |
| Enums | PascalCase for the type; PascalCase for variants | `enum Direction { North, South }` |

### 21.8 Universal Public API Style

The universal API style is the shared design convention for public Zith APIs. It is separate
from a project's feature policy. Multiple implementation styles can expose the same API, as in
the three `classify(score: i32): i32` examples in the README.

Prefer tuples over output parameters for multiple return values. Do not return compile-time
`type` values, raw function pointers, or `dyn fn` directly from ordinary APIs unless the domain
requires them. Use generic trait and interface bounds rather than `dyn` dispatch in the universal
style. Keep `@ensure`, `maybe`, and `assume` internal unless callers need them to understand a
specific API contract. Use `camelCase` for method names.

These are conventions rather than compiler restrictions. A public API may depart from them
when its purpose justifies the added specialization. See
[ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

---

## 22. Appendix — Keyword & Operator Reference

### 22.1 Keywords & Operators

| Keyword | Category | Summary |
|---|---|---|
| `import` / `from` / `export` | Module | Import / inject into scope / re-export. |
| `alias` | Module | Name alias for a type, namespace, or symbol. |
| `use` | Module | Bring a word, context, or operator into scope. |
| `type` | Types | Distinct type copy, or a compile-time constraint (with `or`). |
| `as` | Types | Cast / coercion. Also used in `implement T as Trait`. |
| `is` | Types | Type check / narrowing. Boolean. Supports `@struct`, `@nullable`, etc. |
| `enum` | Types | Closed compile-time constants with one declared value type, or heterogeneous values through `enum: union`. |
| `union` | Types | Runtime-tagged value holding one of its declared member types, which may be heterogeneous. |
| `struct` | Types | Record type. Fields may be grouped with `[]`. |
| `component` | Types | POD / copy-by-default struct. No traits. C-compatible. |
| `implement` | Types | `implement T {}` or `implement T as Trait {}`. |
| `when` | Types | Pattern matching — ranges, type dispatch, branch tags, `..` to ignore fields. |
| `[]T` / `[N]T` / `[_]T` | Types | Slice / fixed array / deduced-size array. |
| `\| \|` | Types | Pack — named tuple / variadic / closure capture group. |
| `pub` / `mod` / `mod(..)` / `mod(N)` | Visibility | Public / module-local, with optional depth. |
| `let` / `var` / `global` / `const` | Bindings | Immutable / mutable / static storage / compile-time constant. |
| `default` / `&T` / `&mut T` / `^T` / `%T` | Memory | Full-Zith ownership and reference forms. `default` is implicit when no modifier is written ([§7](07-memory-model.md), [ADR-0033](adr/0033-nra-reference-model-and-bind.md)). |
| `fn` / `const fn` / `state` / `raw fn` / `extern fn` | Functions | Five exclusive function kinds; cannot be combined. |
| `trait` / `interface` / `extends` / `requires` / `dyn` | OOP | Nominal traits, structural interfaces, extension, constraints, dynamic dispatch. |
| `Copy` / `Functor` / `Arithmetic` | Capabilities | Operator and behavior capabilities. |
| `Failable` / `Invalid` / `Error` | Capabilities | Describe invalid-state analysis, methods available in proven-invalid states, and the marker capability for error values ([§8](08-error-handling.md)). |
| `Allocator` / `Generator` / `Share` / `Lent` / `Trust` / `Unique` / `ThreadBackend` | Capabilities | Memory, runtime protocol, and safety capabilities. Runtime thread backends produce a concrete `Thread<T>` handle for `fork`/`merge`. |
| `state` / `dock` / `jump` | State machines | `state` declarations, a state entry call, and terminating transitions. |
| `fork` / `merge` / `revoke` | Threads | Core full-Zith syntax: create a thread, collect its result, and revoke child access to resources. |
| `spawn` | Threads | Stdlib shorthand for an implicit fork; not a core keyword. |
| `->` / `..` | Chain | Chain flow / placeholder for the previous value. Left-to-right. |
| `,` (in a chain) | Chain | Sub-chain — applies but does not advance the main chain value. |
| `operator` / `token` | Words | Custom operator definition / token word definition ([§16](16-words.md)). Must be defined inside a `context` — global operator overloading is prohibited. |
| `?T` / `T!` | Errors | Deprecated as type wrappers. `T!` remains a function return annotation for inferred invalid states. |
| `try` / `or` | Errors | `try` yields a local, bindable valid/invalid result; `or` evaluates a fallback only after invalidity and retains the last invalid result. |
| Postfix `?` | Errors | Legacy Zith-- Nil propagation. |
| Postfix `!` | Errors | Propagate an operation's invalid state to the enclosing function. |
| `or` | Errors / Loops / Types | Invalid-state fallback / loop fallback / type constraint separator. |
| `must` / `assert` | Errors | `must` guards a failable value; `assert` checks a boolean condition. They are distinct. |
| `raw` | Errors / Raw | Always unchecked, in both debug and release. Compiler warns in release. |
| `unsafe` | Raw | Stronger than `raw`; valid only inside raw contexts. |
| `throw` / `fail` / `resume` | Errors | `fail` captures invalid values whose types implement `Error`; `resume x;` replaces the failed result. |
| `with` / `catch` | Errors | `catch` captures any original invalid value from `with` initialization, not errors from its body. |
| `::` | Operators | Scope resolution — access a shadowed outer name. |
| `and` / `or` / `not` / `xor` | Operators | Logical (English keywords). |
| `&.` / `\|.` / `^.` / `~` / `<<` / `>>` | Operators | Bitwise. |
| `@` / `@<` / `#` | Compiler syntax | `@` for intrinsics and compiler magic, `@<` for full-Zith tags. Zith-- also uses `@` for macro calls. `#` marks variable and field attributes. |
| `extern 'C'` | Interop | C binding — automatic via `.h`, manual, or external. |
| `runtime` / `asm` | Config | `ZithProject` / `ZithFlags` build settings. |
| `assets` | Config | `ZithProject.toml` asset path declarations. |

### 22.2 Compiler Intrinsics

| Intrinsic | Summary |
|---|---|
| `@fields T` | Iterate the fields of a type. |
| `@sizeOf T` | Size of a type, in bytes. |
| `@canonicalType T` | Stable canonical type identity, returned as `u128`. |
| `@hasTrait T, Trait` | Check whether a type implements a trait. |
| `@struct` / `@component` / `@union` / `@enum` | Type-kind checks, used with `is`. |
| `@nullable` | Check whether a type is nullable (`?T`). |
| `@primitive` | Check whether a type is a primitive. |
| `@allocate T, data` | Allocate within the current memory region. |
| `@pack` | Extract or inject a closure's capture pack. |
| `@toStruct pack` | Convert a pack to a plain struct. |
| `@toPack struct, region` | Convert a struct back to a pack. |
| `@appendField Type, name: T` | Add a field to a type being constructed. |
| `@removeField Type, name` | Remove a field from a type being constructed. |
| `@appendMethod Type, fn ...` | Add a method to a type being constructed. |
| `@file` / `@line` / `@fnName` | Location information. |
| `@location` | Rich panic message source. |
| `@ok` | Used with `is` to narrow a `Failable` value to its valid state; the `else` branch proves it invalid. |

### 22.3 Attributes

| Attribute | Summary |
|---|---|
| `#volatile` | The variable is volatile; the compiler must not optimize it away. |
| `#thread_local` | The variable uses thread-local storage. |

---

*Zith Language Specification — Draft v0.9 — Subject to change*
