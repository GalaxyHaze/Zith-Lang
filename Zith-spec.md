# ZITH Language Specification
*Draft v0.9 — 2026*

> A general-purpose, multi-paradigm language built on a small, composable core and a large specialised toolbox.

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
10. [Concurrency & Threads](#10-concurrency--threads)
11. [Comptime](#11-comptime)
12. [Assets](#12-assets)
13. [Raw & Unsafe](#13-raw--unsafe)
14. [Data-Oriented Architecture: ECS & Scenes](#14-data-oriented-architecture-ecs--scenes)
15. [Macros](#15-macros)
16. [Words (Custom Operators)](#16-words-custom-operators)
17. [Contexts](#17-contexts)
18. [C Interop](#18-c-interop)
19. [Project Configuration](#19-project-configuration)
20. [Standard Library](#20-standard-library)
21. [Best Practices & Patterns](#21-best-practices--patterns)
22. [Appendix — Keyword & Operator Reference](#22-appendix--keyword--operator-reference)

---

## 1. Overview & Design Philosophy

### 1.1 What Zith Is

Zith is a statically typed, compiled, general-purpose language. Its primary paradigm is OOP built around traits and interfaces rather than classical inheritance. Functional and procedural styles are first-class citizens.

### 1.2 The Toolbox Philosophy

Zith has a deliberately small, stable core and a large, specialised toolbox.

| Core (always used) | Specialised toolbox (domain‑specific) |
|---|---|
| `struct`, `fn`, `lend`, `view` | `scene` — for games / memory‑intensive domains |
| `trait`, `interface` | `marker`, `dock`, `jump` — for OS / embedded / state machines |
| `?T`, `T!`, `or` | `context`, `word` — for DSLs and APIs |
| `when`, `for`, `->` | `async<Generator>` — for data pipelines |

**The Rule of Three:** if a single function or block uses more than three specialised tools, that is a signal to rethink the design. The core is sufficient for most code.

### 1.3 Design Goals

- Expressive, minimal syntax that favours readability without sacrificing power.
- Memory safety without a borrow checker or GC — via Node Resource Analysis (NRA).
- Composable behaviour through traits, capabilities, and interfaces.
- Static, zero-overhead error handling with rich recovery semantics.
- Compile-time computation (`comptime`) as a first-class feature.
- Low-level control (flow functions, markers, scenes) without sacrificing safety in normal code.
- Extensibility through words and macros — ideally scoped inside contexts, not polluting the global namespace.

### 1.4 Context-Bound Extensibility (Best Practice)

Macros and words should ideally live inside a `context` block. Activating them globally is possible but discouraged — similar to `using namespace std;` in C++.

```zith
// Preferred
use SQL QueryBlock {
    // SQL words and macros active only here
}

// Discouraged
use SQL;   // pollutes the rest of the file
```


### 1.5 Compilation Pipeline & .zir IR

The Zith compiler follows a multi-stage pipeline:

```
SCAN -> SEMA -> SOLVE -> NRA -> HIR -> MIR -> LLVM / NATIVE
                                                  \-> .zir
```

| Stage | Description |
|---|---|
| `SCAN` | Lexical analysis and parsing. Produces an AST. |
| `SEMA` | Semantic analysis — name resolution, type checking, visibility enforcement. |
| `SOLVE` | Generics instantiation, macro expansion, compile-time (`comptime`) evaluation. Fully resolves all types before NRA. |
| `NRA` | Node Resource Analysis — memory ownership, aliasing, and lifetime validation. See §7. |
| `HIR` | High-level IR — desugared, typed, NRA-validated IR. |
| `MIR` | Mid-level IR — optimised, lowered closer to machine semantics. |
| `LLVM` / `NATIVE` | Code generation via LLVM backend or direct native codegen. |

#### .zir Intermediate Representation

`.zir` is Zith's binary intermediate representation. It is the distribution format for libraries and VM execution.

- **As a library:** `.zir` can be statically or dynamically linked. The consumer decides the linkage mode.
- **As an executable:** `.zir` files carry a header with metadata (entry point, required features, ABI version). A Zith VM can interpret them directly.
- **As a compilation target:** `.zir` can be passed to the LLVM backend for native code generation.

This makes `.zir` the universal exchange format — distribute once, consume however the client prefers.

---

## 2. Module System

### 2.1 Import Keywords

| Keyword | Behaviour |
|---|---|
| `import path opt‑in(as name)` | Imports a module; path becomes its namespace or you access via `name.symbol`. |
| `from path` | Injects all public symbols directly into scope (sugar for beginners). |
| `export path` | Re‑exports a dependency; consumers of this module also receive it. |

```zith
import std/io/console as console;
@console.println("hi");

from std/io/console;
@println("hi");

export std/io/console;
```

### 2.2 `alias` vs `use`

| Keyword | Purpose | Example |
|---|---|---|
| `alias` | Create a name alias for a type, namespace, or symbol. | `alias Vec = std.collections.DynArray;` |
| `use` | Bring a word, context, or operator into the current scope. | `use DOT = math.vec.dot;` / `use SQL;` |

```zith
alias Vec   = std.collections.DynArray;
alias print = std.io.console.println;

use math.vec.dot;
use SQL;
```

### 2.3 Namespace Access & Scope Resolution

Namespaces are accessed with `.` — e.g. `std.io.console.println`. The `::` operator reaches upward past a shadowed name to the outer scope.

```zith
let x = 10;
{
    let x = 20;
    @println(::x);   // 10 -- outer scope
}
```

### 2.4 Type Constraints vs Union Separators

| Construct | Separator | Semantics |
|---|---|---|
| Type constraint | `or` (keyword) | Compile‑time template restriction. Dispatched at compile time. |
| Union body | `,` (comma) | Runtime tagged union. Variants separated by commas. |

```zith
// Type constraint -- compile-time dispatch
type Number = i32 or f64 or bool;
fn convert<T: Number>(val: T): string { ... }

// Union -- runtime tagged
union Number { i32, f64, bool }
```

### 2.5 Visibility

| Modifier | Scope |
|---|---|
| *(none)* | Private — visible only within the declaring file. |
| `pub` | Public — visible to any importer. |
| `mod` | Module‑local — visible to immediate sibling files in the same directory. |
| `mod(..)` | Visible to all sub‑directories, unlimited depth. |
| `mod(N)` | Visible to exactly N levels of sub‑directories deep. |

---

## 3. Type System

### 3.1 Primitive Types

| Category | Types |
|---|---|
| Unsigned integers | `u8`, `u16`, `u32`, `u64`, `u128` |
| Signed integers | `i8`, `i16`, `i32`, `i64`, `i128` |
| Floats | `f32`, `f64` |
| Other primitives | `bool`, `char`, `void` |
| Compiler‑internal | `unknown` — valid but unresolved type (not user‑instantiable). `invalid` — dead/uninitialised state (moved variable, proven‑null). Neither can be named or stored by user code. |
| Special | `noreturn`, `null`, `File`, `Folder` |
| Opaque | `opaque` — non‑owning tagged `void*`. `unique opaque` — owns the pointed‑to value. `raw opaque` — untagged `void*` (C interop). |

### 3.2 Slice & Array Types

| Syntax | Meaning |
|---|---|
| `[]T` | Slice — fat pointer (pointer + length). String literals are `[]char`. |
| `[N]T` | Fixed‑size array, stack‑allocated. |
| `[_]T` | Deduced‑size array — compiler infers N from the initialiser. |

#### Strings & Origin Tracking

NRA tracks the **origin** of every string node (`literal`, `allocator`, `stack`):

```zith
// []char implicitly casts to string -- zero cost (literal origin)
let s: string = "hello";

// Concatenation changes origin to allocator -- triggers allocation
let greeting = "hello" + " " + "world";
```

> `string` is a built‑in library type. `[]char` is the primitive representation.

### 3.3 Enum

A closed set of named constants. All values must be known at compile time. Three styles:

#### C‑style
```zith
enum Direction { North, South, East, West }
enum Status: i32 { Ok = 0, Err = 1, Pending = 2 }
```

#### Struct‑backed
```zith
enum Color: rgb {
    red   = { r: 255, g: 50,  b: 0,   a: 255 },
    green = { 0, 255, 0, 255 },
}
```

#### ADT‑style (Rust‑like)
```zith
union Shape {
    Circle = { radius: f32 },
    Rect   = { w: f32, h: f32 },
    Point,
}

enum Constants: union {
    pi      = 3.14f,
    vector  = |x: -1, y: 0, z: -1, w: 1|,
    nothing = 0
}

fn area(s: Shape): f32 {
    when (s) {
        Circle = s.radius * s.radius * Constants.pi,
        Rect   = s.w * s.h,
        Point  = Constants.nothing,
    }
}
```

### 3.4 Struct

#### Field Declaration & Grouping

Individual fields for unrelated members; `[]` groups for semantically related fields of the same type:

```zith
struct Sample { name: string, age: i32 }

struct Point { [x, y, z]: f32 }

struct Transform {
    [x, y, z]:    f32,   // position
    [rx, ry, rz]: f32,   // rotation
}
```

#### Generic Structs & Self‑referential Patterns

```zith
struct Pair<T, U> { first: T, second: U }

// Doubly‑linked list node
struct Node<T> {
    data: T,
    next: ?unique Self,   // owns next; null at tail
    prev: ?belong Self,   // back‑ref; lifetime tied to parent; null at head
}
```

#### Implementation Blocks

```zith
implement Node<T> {
    fn isHead(self): bool { self.prev is null }
    fn isTail(self): bool { self.next is null }
}

implement Node<T> as Printable {
    fn print(self) { @println("Node({self.data})"); }
}
```

> A struct body may declare method signatures without bodies. Those must then be implemented in an `implement` block.

#### `self`, `other`, `Self`

`self` is the current instance. `other` is a conventional second instance parameter. `Self` (capitalised) refers to the concrete type being implemented.

### 3.5 Component

A plain‑old‑data (POD) struct. **Copy by default** (alongside primitives). Cannot implement traits. May have inline functions limited to pure transformations. Represents a C‑compatible struct.

```zith
component Vec2 {
    [x, y]: f32,
    fn length(self): f32 { sqrt(self.x*self.x + self.y*self.y) }
    fn dot(self, other: Self): f32 { self.x*other.x + self.y*other.y }
}
```

A component must satisfy all of the following constraints:
- All fields are primitives or other components (no structs, unions, enums that aren’t integers).
- No trait implementations (no `implement ComponentName as Trait`).
- Inline functions allowed, but can only perform pure transformations:
  - No side effects (I/O, allocation, global mutation).
  - Only arithmetic, comparisons, and field access.
  - Must return a value; cannot return `void`.
- Copy is always bitwise (memcpy‑safe).
- Layout is C‑compatible (no vtable, no fat pointers).
- Cannot contain self‑referential fields (`?unique Self`, `?belong Self`).

### 3.6 Union

By default a `union` is runtime tagged. Variants separated by commas.

```zith
union Value { i32, f64, bool }

// Type hint forces union deduction
let x: union = when (flag) { 0 = 42, 1 = 3.14, 2 = true };
```

> Without the `union` type hint the compiler cannot deduce a union — it must be stated explicitly.

`raw union` is a C‑union, only valid inside `raw` contexts. Accessing the wrong variant is undefined behaviour.

### 3.7 Packs (`| |`)

A **Pack** unifies variadics, named tuples, and explicit closure captures. Internally a closure is a pack plus a static function or a function pointer.

```zith
// Bare pack -- named tuple
let pos  = |x: 1.0, y: 2.0, z: 0.0|;
@println(pos.x);       // named access
@println(pos.0);       // positional access

// Closure = pack + () + body
// Move by default
let addBase = |view base| (n: i32): i32 { base + n };

// Explicit capture, move 'base' into the closure
let f = |base| (n: i32): i32 { base + n };
```

#### `static` vs `dyn`

By default a closure generates an **inline** (static) call. The `dyn` qualifier makes it an **indirect** call via function pointer. The capture pack signature is still known at compile time unless `opaque` is used.

```zith
let closure: dyn = |base| { ... };

// Extract / inject the pack at compile time
let pack = @pack closure;
@pack closure = pack;
```

#### Dynamic Levels

| Level | Syntax | Semantics |
|---|---|---|
| Full static | `let f = \|x\| x + 1;` | Zero overhead. |
| Semi‑dynamic | `let f: dyn = \|x\| x + 1;` | Function pointer; capture signature statically known. |
| Dynamic pack, static fn | `let f: \|opaque\| = \|x\| x + 1;` | Capture pack opaque, no type info inside. |
| Full dynamic | `let f: dyn \|opaque\| = \|x\| x + 1;` | Both function and capture opaque; full fat pointer. |

> Closures are detailed here; see §5 for function kinds.

### 3.8 Union Narrowing (`is`) & Flow Typing

The `is` operator narrows a union within a branch. Inside each branch, invalidations (mutations, moves) do not affect other branches. After the if/else or when block, all changes and side-effects are applied to the enclosing scope and the type **widens back** to the full union.

If you want to "recover" a narrowed value, make the if return and store it. The compiler deduces "not all control flows return" to Optional — when a branch does not return, it returns null.

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

// Widening -- mutation inside branch resets type
when (v) {
    i32 = { v = 42; },   // v mutated to i32
}
// v is Val again here (widened back to full union)

// Recovering a narrowed value via if-return
let result = if (v is i32) { v } else { null };
// result is ?i32 -- null when branch did not return

// when with branch tags
when (v) {
    n: i32  = @println("int: {n}"),
    f64     = @println("float: {v}"),
    _       = @println("other"),
}

when (shape) {
    Circle = @println("circle r={shape.radius}"),
    Rect   = let [val..] = shape,   // ignore second field
    _      = @println("other"),
}

// Standalone boolean & compile‑time reflection
// All conditions / booleans tests must be inside '()'
let numeric  = (v is i32) or (v is f64);
let isStruct = (T is @struct);
```

### 3.9 Generics

```zith
// Explicit
fn serialize<T: Serializable + Printable, U: Clone>(val: T, ctx: U): string { ... }

// Implicit -- constraints inferred from usage at call site
fn add(a, b) { a + b }   // Arithmetic implicitly required
```

### 3.10 `when` — Pattern Matching

```zith
when (count) {
    0       = @println("none"),
    1       = @println("one"),
    2..10   = @println("few"),
    _       = @println("many"),
}

// As expression
let label = when (score) { 90..100 = "A",  70..90 = "B",  _ = "C" };

// .. ignore the rest
when (point) {
    [x..] = @println("x=", x),
    _         = @println("no match"),
}

// .. get the last element
when (point) {
    [..w] = @println("w={w}"),
    _         = @println("no match"),
}
```

### 3.11 Cast Operator

```zith
let n: i32 = 42;
let f = n as f64;
```

---

## 4. Traits, Interfaces & Capabilities

### 4.1 Traits vs Interfaces

| | Trait | Interface |
|---|---|---|
| Typing | Nominal — must be explicitly implemented. | Structural (duck‑typed) — auto‑satisfied if members match. |
| Can extend? | Yes (`requires` / `extends`). | can `requires`. |
| Has implementation? | Yes (default methods allowed). | No — declaration only. |
| Field access? | Only via `requires Interface`. | Yes — directly. |

### 4.2 Traits

- `requires Cond` appears **before** the `trait` keyword. Forces any implementing type to already satisfy that trait or interface.
- `self` / `other` are special parameter keywords. `Self` is the concrete implementing type.
- Traits may provide default method implementations.
- For ambiguous method names, disambiguate with the trait as namespace: `TraitName.method(self, ...)`

```zith
trait Printable {
    fn print(self);
    fn println(self) { self.print(); io.writeln(""); }
}

requires Printable
trait JsonSerializable {
    fn print(self);
    fn toJson(self): string;
}

// Disambiguate overlapping method names
Printable.print(self);
JsonSerializable.print(self);
```

### 4.3 Interfaces

Interfaces are structural: a type satisfies an interface automatically if it has the required members. A trait that `requires` an interface gains read/write access to those fields.

```zith
interface iPositioned { [x, y, z]: f32 }

requires iPositioned
trait Movable {
    fn translate(self, dx: f32, dy: f32, dz: f32) {
        self.x += dx; self.y += dy; self.z += dz;
    }
}

// Any struct with x, y, z: f32 satisfies iPositioned automatically
struct Enemy { [x, y, z]: f32, health: i32 }
```

### 4.4 Capabilities — Built‑in Reference

Capabilities are special traits consumed by the compiler to unlock operations, enforce rules, or modify NRA behaviour.

| Capability | What it enables |
|---|---|
| `Copy` | Implicit bitwise copy. Components and primitives are Copy by default. |
| `Functor` | `operator()` — makes the type callable like a function. |
| `Arithmetic` | Operators `+`, `-`, `*`, `/`, `%`, etc. |
| `Error` | `operator throw` — required for `throw MyError;` |
| `Null` | Activates associated traits only when NRA has proven the value IS null. Negative capability — fires on proven‑invalid state. |
| `Fail` | Activates associated traits only when NRA has proven the value IS an error/broken. Cannot coexist with `Null` on the same value. |
| `Allocator` | Custom allocator backend for Scenes and `@allocate`. `delete fn free;` makes a bump‑only allocator. |
| `Generator` | Enables `async<Generator> fn`. Only meaningful on async functions. |
| `Share` | Required for `global: share` and thread‑crossing. **share is mutable** — does not imply read‑only. |
| `Lent` | Enables `global: unique` runtime‑checked exclusive borrow. |
| `Trust` | A trait extending `Trust` may contain `raw fn` methods callable from safe contexts. |
| `Unique` | Singleton type. Cannot be instantiated; type name acts as the instance. All fields implicitly global; mutable fields require thread‑safety. |

#### Null & Fail — Negative Capabilities

```zith
implement Config as Null {
    fn onMissing(self) { log("Config was null -- using defaults"); }
}
implement Config! as Fail {
    fn onError(self) { log("Config load failed"); }
}

let cfg: ?Config = loadConfig();
if (cfg is null) {
    cfg.onMissing();   // Null trait unlocked here
}
```

#### Trust — Safe Sections with Raw Code

```zith
trait Place extends Trust {
    raw fn sample(): i32 {}
}

fn safe_caller(a: impl Place) {
    let v = a.sample();   // allowed: Trust in scope via Place
}
```

#### Unique — Singleton Types

```zith
struct AppConfig: Unique { host: string, port: u16 }

AppConfig.host = "localhost";
AppConfig.port = 8080;
// let cfg = AppConfig { ... };  -- COMPILE ERROR
```

#### Share — Thread‑crossing Mutable State

```zith
// Share is mutable -- multiple names, multiple threads, all can write
global counter: share AtomicI32 = 0;

// Without Share capability, cross‑thread access is a compile error
struct LocalOnly { data: i32 }
// global bad: share LocalOnly = ...;  -- COMPILE ERROR: lacks Share
```

### 4.5 Operator Overloading

Capability‑based and strict:

```zith
implement Vec3 as Arithmetic {
    fn +(self, other: Self): Self { ... }
    fn -(self, other: Self): Self { ... }
}

implement Pipeline as Functor {
    fn ()(self, input: []u8): []u8 { ... }
}
let out = pipe(raw_bytes);
```

### 4.6 Extension (`extends`)

`extends` copies the base type's fields and traits into the new struct. After `:`, additional traits to implement may be listed:

```zith
struct Dog extends Animal {}
struct T extends Base: Transform, Collision {}

// Traits may extend capabilities or other traits
trait SafeBuffer extends Trust {
    raw fn readByte(self, offset: u64): u8 {}
}
```

---

## 5. Functions

### 5.1 Return Type & Implicit Returns

```zith
fn add(a: i32, b: i32): i32 { a + b }   // explicit, implicit return
fn add(a: i32, b: i32)      { a + b }   // inferred type

// By default, bound‑check: if ok, return normally; if not, propagate as 'null'
fn first<T>(slice: []T): ?T {
    slice[0]?
}
```

> The compiler cannot infer a union return type without an explicit `union` type hint.

### 5.2 Function Kinds

| Kind | Description |
|---|---|
| `fn` | Standard runtime function. |
| `const fn` | Resolved at compile time. |
| `async fn` | Coroutine (default). |
| `async<Generator> fn` | Explicit generator — yields a typed sequence. |
| `flow fn` | Enables marker/dock control flow. |
| `raw fn` | Always unchecked — bypasses safety in both debug and release. Compiler warns in release if `raw` could be removed. |

> Function kinds are orthogonal and cannot be mixed.

**Macro calls use `@` prefix** — `@println`, `@log`, `@serialize`. Functions use bare names — `console.write`, `process`, `save`.

### 5.3 `async fn` & `yield`

```zith
// Default coroutine
async fn fetch(url: string): Response! {
    yield;
    get_response()!
}

// Explicit generator
async<Generator> fn range(start: i32, end: i32): ?i32 {
    for (i in start..end) {
         yield i;
         i += 1;
    }
}

for (v in range(0, 10)) { @println(v); }
```

### 5.4 Closures

A closure is a pack with callable semantics. Capture modes and dynamic levels are detailed in §3.7. Briefly:

```zith
let addBase = |base| (n: i32): i32 { base + n };
let f1 = |view x| (n: i32) { x + n };
let f2 = |x| (n: i32) { x += n; };
```

---

## 6. Mutability & Bindings

### 6.1 Deep Mutability Model

Zith uses deep mutability: the modifier on a binding propagates into all nested fields. Fields inside a struct are `auto` by default — they follow the mutability of the containing instance.

### 6.2 Binding Keywords

| Keyword | Controls | Semantics |
|---|---|---|
| `let` | Binding | Immutable. Cannot be reassigned. |
| `var` | Binding | Mutable. Can be reassigned. |
| `global` | Binding | Static storage duration. |
| `const` | Both | Compile‑time constant. Binding and content frozen. |
| `comptime` | Both | Mutable during compilation (e.g. compile‑time counter), frozen at runtime. |

> Bindings only control reassignability. Content mutability is controlled via memory keywords on the type (see §7).

```zith
let x: mut Point;      // cannot reassign x, but Point fields are mutable
var y: Point;          // can reassign y, Point fields immutable
const PI = 3.14159;
comptime COUNT = 0;
comptime COUNT += 1;   // valid at compile time only
```

### 6.3 Destructuring

```zith
let [x, y, z]: f32;           // grouped -- same type, related semantics
let name: string; let age: i32;   // individual -- unrelated

// In a for loop, if loop doesn't run, 0 is the default
let r = for ([acc, i]: i32), (i in 0..n) {
            acc *= i + 1
        } or 0;
```

---

## 7. Memory Model (NRA)

### 7.1 What NRA Does (and Does Not Do)

Node Resource Analysis (NRA) is the final compiler pass, after SEMA + SOLVE have fully resolved all types. It is responsible for:

- Tracking whether a node (value) is `alive`, `dead`, or `lent`.
- Ensuring ownership rules: `unique` nodes have exactly one owner; `belong` nodes cannot outlive their parent.
- Validating aliasing: `share` allows multiple names; `lend` is exclusive.
- Tracking node origins (`literal`, `allocator`, `local`, `view`) to enable zero‑cost coercions.

NRA does **not** infer types — that is SEMA/SOLVE's job.

### 7.2 Move Semantics

Moving `a` to `b` redirects the name `b` to `a`'s node. The name `a` is set to **dead** and cannot be read — only reassigned:

```zith
var a = Point { x: 1.0, y: 2.0 };
let b = a;                          // b -> a's node; a becomes dead
// println(a.x);                    -- COMPILE ERROR: a is dead
@println(b.x);                       // OK

a = Point { x: 3.0, y: 4.0 };      // OK: reassignment creates new node for a
```

Effectively, if a is not reassigned, it is as if a never existed and b has always held `Point{x: 1.0, y: 2.0}`.

### 7.3 Memory Keywords

| Keyword | Relationship | Common use |
|---|---|---|
| `default` | Owned. Lifetime follows the binding. | Variables, struct fields |
| `lend` | Exclusive mutable temporary. Cannot be stored, moved, or captured. **Can be returned** (passes the behavioural promise to the caller). **Belong fields can be passed as `lend` to functions.** | Passing mutable refs to fns |
| `view` | Read‑only non‑owning ref. Many views may coexist. | Inspecting without ownership |
| `unique` | Single‑owner guarantee. Only one name in the graph. | Ownership‑transfer patterns |
| `share` | Multiple names, same node. Statically validated — no ref‑count. Mutable. | Compile‑time‑proven sharing |
| `belong` | Part‑of. Node lifetime tied to parent. Cannot be stored independently. Can be passed as `lend` to functions. | Back‑pointers, hierarchies |

> In practice, most code only needs `lend` and `view`.

### 7.4 The Four NRA Rules

**Rule 1 — Argument Exclusivity:** in any call expression, each argument must refer to a distinct node. No exceptions.
- `default` / `unique` / `lend` duplicated → **ownership error**
- `share` / `view` duplicated → **logic error** (same resource in two argument positions is almost certainly a bug)

**Rule 2 — No Dead Node Access:** a symbol may not be read if its node is in state `dead`.

**Rule 3 — No Escaping `belong`:** a `belong` node may not be stored in any location whose lifetime exceeds any node in its `dependency nodes`. At every use, all parents must be `alive`.

**Rule 4 — `lend` Behavioural Promise:** a `lend` value may not be stored, moved, or captured. It may be passed as a call argument or returned (passing the promise to the caller).

### 7.5 NRA in Practice

```zith
// lend -- exclusive temporary borrow
fn scale(p: lend Point, factor: f32) { p.x *= factor; p.y *= factor; }
let pt = mut Point { x: 3.0, y: 4.0 };
scale(pt, 2.0);
@println(pt.x);   // OK: borrow ended

// view -- multiple read‑only refs
let v1: view Point = pt;
let v2: view Point = pt;   // fine

// share -- no ref‑count, statically proven
let a: share Config = load();
let b: share Config = a;   // both point to same node

// belong -- back‑pointer cannot outlive parent
struct Tree<T> {
    data:     T,
    children: []unique Self,
    parent:   ?belong Self,
}

// belong fields passed as lend
fn getParent(self: view Node): lend Node { self.parent }
```

### 7.6 NRA Limitations & Escape Hatches

NRA cannot statically validate all shared / view cycles across threads:

```zith
// await -- safe: compiler knows thread is done before scope ends
let handle = spawn worker(shared_data);
await handle;

// #wontRemain -- promise: thread dies before scope ends
#wontRemain let _ = spawn quick_task(shared_data);

// Rc -- runtime ref‑count fallback
let shared = Rc.new(HeavyResource.init());
let _ = spawn worker(Rc.clone(shared));
```

### 7.7 Self‑referential Types

```zith
struct Node<T> {
    data: T,
    next: ?unique Self,
    prev: ?belong Self,
}

implement Node<T> {
    fn append(self: lend Self, data: T) {
        self.next = unique Node { data, next: null, prev: belong self };
    }
}
    // Freeing the head frees the entire chain (unique ownership chain)
    // NRA guarantees prev (belong) never outlives its owner
    // belong fields can be passed as lend to functions
}
```

### 7.8 NRA Algorithm

NRA maps every symbol or variable to a **resource node**. By default, the compiler is **lazy** — it only validates a node when it is used, viewed, or returned.

#### Node Validation

When a node is accessed, NRA checks:
1. The node itself is `alive` (not `dead`).
2. All nodes in its **dependency vector** (the fields or resources it belongs to or references) are valid.

If a node is set to `dead` (e.g. after a move), the compiler inserts a diagnostic message recording **where** the node died and **why**. This information is used to produce precise NRA errors at the point of violation.

#### Function Evaluation

When NRA evaluates a function, it first checks: **was this function already analysed?** If yes, the cached result is reused.

Otherwise, it inspects all return paths:
- If **every** return path returns one of the function's arguments, then at the call site that node is **not consumed** — ownership stays with the caller.
- If any path does **not** return an argument, the result is marked as **consumed**.

Since memory modifiers (lend, view, unique, etc.) are part of the function signature, the compiler has all the information it needs to determine consumption.

#### Branch Isolation (`if` / `else` / `when`)

Inside conditional branches, each branch operates in **isolation** — it cannot see what other branches do:
- A move, mutation, or invalidation inside one branch does **not** affect other branches.
- After all branches complete, NRA **collects all side-effects** and applies them to the enclosing scope.

#### The Return Trick

If a conditional expression is used to recover a value but not all paths return, the compiler implicitly deduces `null` for the missing paths. The result type becomes `?T`:

```zith
let result = if (v is i32) { v } else { };     // ?i32
```

> **Name linking:** The `fail` block name must match the enclosing block name. The compiler passes the error as the block's argument. An anonymous `fail` (no name) guards the current scope directly and receives the error the same way.

---

## 8. Error Handling

Fully static, return‑based. No exceptions. No semicolon required after `?` or `!`.

### 8.1 Failable Types

| Syntax | Meaning | Propagated by |
|---|---|---|
| `?T` | Optional — T or null. | `?` (postfix) |
| `T!` | Result — T or an error. Equivalent to Rust's `Result<T, E>` where `E` must implement the `Error` trait. Compiler infers anonymous error union for multiple error types. | `!` (postfix) |

Failable types may be stacked — notation is linear and unambiguous:

```zith
// Optional pointer to an optional Result of optional i32 or IoError
?*?(?i32 ! IoError)
```

### 8.2 `must` vs `raw`

| | Debug mode | Release mode |
|---|---|---|
| `must` | Panics with file + line location. | Compiler guides you to remove, becomes an if/else + early return with a custom error code. |
| `raw` | Always unchecked. | Always unchecked. |

```zith
let cfg: ?Config = tryLoad();
let c1 = must cfg;   // panics debug; compiler warns/guides in release
let c2 = raw cfg;    // always unchecked; compiler always warns
```

### 8.3 Propagation & Fallback

```zith
fn readConfig(path: string): Config! {
    let file = File.open(path)!
    let data = file.read()!
    parse(data)!
}

let name = ?user.name or "guest";
let data = !primary() or backup() or default;

// Propagation inside chain flow
readFile("data.bin") -> parse(..)! -> validate(..)? -> process(..)
```

`?` / `!`, `raw`, `must` are needed to access Failable types:
- **?** : If the value is invalid (null), fall back to the `or` alternative. The `or` value becomes the **Integral** — the valid, unwrapped T.
- **!** : If the value is invalid (error), fall back to the `or` alternative. The `or` value becomes the Integral.

> **Integral** = the valid, non-null, non-error value of a failable type — the inner T that `?` and `!` unwrap to.

> They are one per 'island': `let x = ?Opt or default;`, not `let x = ?Y.?data.?fic();` nor `let x = ?Opt?`.

### 8.4 `with` / `catch`

```zith
// Short‑circuit: first failure jumps to catch
with (connectDb(), user: getUser(id)) {
    process(user);
} catch (err) { log(err); }

// Eager: all expressions evaluated; catch runs if any failed
with! ( a:fetchA(), b: fetchB() ) { use(b); }
catch { log(a,b); }
```

### 8.5 `fail` Blocks

A `fail` block runs when an error would escape the associated scope. Can appear after a named block (external) or inside a block (nameless scope guard):

```zith
// External fail
loadConfigure {
    let raw = readFile("config.json")!
    parse(raw)!
} fail loadConfigure(err) {
    if (err is NotFound) { continue(default); }
    throw Error{ context: "load failed", cause: err };
}

// Nameless fail -- guards current scope
{
    fail (err) { log("scope error:", err); }
    risky()!
    another()!
}
```

Options inside a `fail` block:
- `continue(value)` — resume after the block with a replacement value.
- `return value;` — exit the enclosing function.
- `throw value;` — propagate a new error (requires `Error` capability).
- Fallthrough — original error propagates unchanged.

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

Parentheses `()` are mandatory on all control structure conditions except function calls. Logical operators use English keywords; bitwise use standard symbols followed by `.`.

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
for (i = 0), (i < 10), (i += 1) { ... }       // init / cond / step
for (v in range(0, 100)) { @println(v); }       // over a generator

// Destructure group with fallback
let r = for ([acc, i]: i32), (i in 0..n) { acc *= i + 1 } or 0;
```

> If the loop may not run, the return is deduced as optional — unless `or` collapses it to non‑optional.

> The init/cond/step form uses comma-separated parenthesised expressions: `for (i = 0), (i < 10), (i += 1)`. An alternative flat form is also accepted: `for (i = 0, i < 10, i += 1)`.

### 9.3 Chain Flow (`->`)

The `->` operator pipes output left‑to‑right. Previous value is `..`. Tags capture values for later. `!` and `?` propagate out of the chain normally. Precedence: left‑to‑right, lower than function calls.

```zith
getData() -> process(..) -> save(..);

getData()
    -> raw:    parse(..)
    -> parsed: validate(..)!       // ! propagates out
    -> connectDb()
    -> save(parsed);

// Inline block
readFile("data.bin")
    -> { let h = parse_header(..); validate(h)! }
    -> process_body(..);

// Comma sub‑chain -- f1, f2 receive foo's value but do NOT advance the chain
foo(), f1(..), f2(..) -> f3(..);

// Parenthesised real sub‑chain
foo(), (f1(..) -> f2()) -> f3(..);
```

> Comma sub‑chains are useful for side‑effects (logging, validation) without disrupting the main data flow.

### 9.4 `flow` Functions & Markers

```zith
flow fn run(data: Stream): void {
    marker Process(chunk: Chunk, count: i32) {
        transform(chunk);
        // count preserved from last invocation if not updated
    }

    stackful marker Stack() {
        // stackful behaviour — locals dropped before jump
    }

    let i = 0;
    for item in data {
        dock { jump Process(item, i); }
        i += 1;
    }
}

// Global marker
marker ContextSwitch(next: TaskId) {
    saveRegisters();
    loadTask(next);
}

// noreturn: dock return variable never modified; calling dock can never resume
flow fn scheduler(): noreturn { ... }
```

**Marker rules:**
- Hoisted to top of flow fn; skipped during normal execution.
- Cannot access external variables or declare their own locals.
- Arguments live in a `thread_local` blob. Bits preserved across invocations unless explicitly updated.
- It can receive any thing from outer scope (dock).
- Global markers may call regular functions, but NOT `flow`‑controlled functions — unless they return `noreturn`.

#### Stackful Markers (opt‑in)

By default, markers are **stackless**. You can opt into stackful markers with the `stackful` modifier. When a `jump` is executed from inside a stackful marker, all local owned nodes are dropped (cleaned up) before the jump. Only owned (move/copy) or `view`/`lend` values may cross a `jump` — `lend` of a local is forbidden as it would not survive the cleanup.

```zith
flow fn run(data: Stream): void {
    stackful marker Process(chunk: Chunk) {
        let buffer = allocate(chunk.size);  // owned -- dropped before jump
        transform(buffer);
        // jump here drops buffer, only chunk crosses
    }
}
```

---

## 10. Concurrency & Threads

### 10.1 Spawning

```zith
// Default thread type
let handle = spawn worker_fn(data);

// Explicit thread type capability
let handle = spawn GreenThread fn worker_fn(data);

// Fire‑and‑forget
let _ = spawn background_task();

// #wontRemain -- promise the thread dies before scope ends
#wontRemain let _ = spawn quick_task(shared_data);
```

### 10.2 Compile‑time Safety Enforcement

If a thread accesses shared data, the compiler requires **one of**:
- `await handle` before the shared data goes out of scope.
- `#wontRemain` attribute on the spawn.

Missing both is a **compile error**. Violating the `#wontRemain` promise is not caught — the shared data may become a dangling reference.
> One alternative is use `global: share T` or use `Rc`.

| Keyword / Method | Semantics |
|---|---|
| `await handle` | Waits for the thread to finish. |
| `await globalVar` | Blocks until the global variable has been initialised. |
| `handle.send(msg)` | Sends a message to the running thread. |
| `#wontRemain` | Attribute on spawn. Promise: thread dies before enclosing scope ends. |

```zith
let handle = spawn worker(shared_data);
await handle;

global cfg: Config;
await cfg;
@println(cfg.host);
```

---

## 11. Comptime

Comptime covers compile‑time computation: reflection, type manipulation, and `const` blocks.

### 11.1 Comptime Bindings

The `comptime` keyword makes a binding mutable during compilation and frozen at runtime:

```zith
comptime counter = 0;
counter += 1;   // valid at compile time
// counter += input().nextI32();          // COMPILE ERROR: frozen at runtime
```

### 11.2 `const` Blocks

`const {...}` executes code at compile time. All values inside must be computable at compile time — if any depend on runtime input, the compiler emits an error.

`const fn` functions are forced to resolve everything at compile time and **must** be called inside a `const` block (or assigned to a `const` binding). They cannot be called at runtime.

```zith
const result = {
    let x = 10;
    let y = 20;
    x + y   // evaluated at compile time
};

import assets/data.json as Data;
const fn processJson(data: []char): JsonValue { ... }
const parsed = processJson(Data);  // runs at compile time
```

> Some macros / functions are overloaded at compile‑time; a compile‑time throw will emit the error message.

### 11.3 Reflection

Use `@` intrinsics to inspect types at compile time:

```zith
// Iterate fields of a struct
for ( member in @members(MyStruct) ) {
    @println("{}: {}", member.name, member.type);
}

// Check type kind
let isPrim = (T is @primitive);    // bool, i32, f64, etc.
let isStr  = (T is @struct);       // struct
let isComp = (T is @component);    // component
let isUn   = (T is @union);        // union
let isEn   = (T is @enum);         // enum

// Get field visibility
for ( field in @fields(MyStruct) ) {
    @println("{}: {}", field.name, field.visibility);  // pub, mod, private
}

// Check nullability
let nullable = (T is @nullable);   // ?T
```

### 11.4 Type Manipulation

You can create types and modify them before they are “finalised”:

```zith
// Create a new type
type Custom = @struct;

// Add fields -- allowed while type is not yet returned/instantiated
@appendField Custom, x: i32;
@appendField Custom, y: f32;

// Add methods
@appendMethod Custom, fn distance(self): f32 { sqrt(self.x*self.x + self.y*self.y) }

// Type is "done" once returned or instantiated; passing to generic functions also counts
let p: Custom = Custom { x: 1, y: 2.0 };

// Primitive aliases are IMMUTABLE -- cannot add fields
type Celsius = i32;   // Celsius is i32, no fields to modify
@appendField Celsius, x: i32;  // COMPILE ERROR: type is 'done' (primitive)
```

> A type is “done” when you return it or instantiate it. Until then, you can add/remove fields and methods.

---

## 12. Assets

Assets are external files (JSON, images, data) that the compiler validates and makes available at compile time.

### 12.1 Configuration

Declare asset paths in `ZithProject.toml`:

```toml
[project]
name = "my_game"
version = "0.1.0"

[assets]
assets = ["assets/", "../someOtherFolder"]
```

The compiler checks that all paths exist and are readable at compile time.

### 12.2 Importing Assets

```zith
import assets/data.json as Data;
import assets/sprites/player.png as PlayerSprite;
```

> Using `as` is mandatory to avoid conflicts between files with the same name but different extensions.

Assets are available as compile‑time constants.

### 12.3 Processing Assets at Compile Time

Combine with `const fn` to process assets at compile time:

```zith
import assets/config.json as ConfigData;

const fn parseConfig(data: []char): Config {
    // parse JSON at compile time
    JSON.parse(data)!
}

const CONFIG = parseConfig(ConfigData);   // runs at compile time
```

### 12.4 Runtime Assets

For assets loaded at runtime (not declared in `[assets]`), use standard file I/O:

```zith
let runtimeData = fs.read("runtime/save.json")!;
```

---

## 13. Raw & Unsafe

### 13.1 Safety Hierarchy

```
Safe code (default)
    └─ raw fn / raw blocks (always unchecked)
         └─ unsafe keyword (only inside raw contexts, stronger bypass)
```

| | `raw` | `unsafe` |
|---|---|---|
| **What it is** | Base‑level bypass. Always unchecked. | Stronger bypass. Only valid inside `raw` contexts. |
| **Scope** | Any expression or statement. | Only inside `raw fn` or `raw` blocks. |
| **Debug mode** | Unchecked. | Unchecked. |
| **Release mode** | Unchecked. Compiler warns. | Unchecked. Compiler warns. |
| **Use case** | C interop, performance‑critical paths. | Operations that are undefined if misused (pointer arithmetic, inline assembly). |

### 13.2 `raw fn`

A `raw fn` bypasses safety checks in both debug and release. The compiler warns if `raw` could be removed in release builds.

```zith
raw fn c_compat(x: opaque): opaque {
    // no safety checks, no bounds checking, no null checks
    x
}
```

### 13.3 `unsafe` Keyword

`unsafe` provides a stronger bypass than `raw`. It is only valid inside `raw` contexts (`raw fn` or `raw` blocks). Use `unsafe` for operations that are undefined if misused: pointer arithmetic, inline assembly, accessing hardware registers.

```zith
raw fn dangerous(x: opaque) {
    unsafe {
        // pointer arithmetic -- undefined if x is invalid
        let ptr = x as *i32;
        *ptr = 42;

        // inline assembly
        asm("mov rax, {x}", x = x);
    }
}
```

### 13.4 `Trust` Capability

`Trust` bridges safe code to raw/unsafe. A trait extending `Trust` may contain `raw fn` methods callable from safe contexts:

```zith
trait Place extends Trust {
    raw fn sample(): i32 {}
}

fn safe_caller(a: impl Place) {
    let v = a.sample();   // allowed: Trust in scope via Place
}
```

### 13.5 When to Use Each

| Situation | Use |
|---|---|
| C interop, headerless libraries | `raw fn` |
| Pointer arithmetic, inline assembly | `unsafe` inside `raw fn` |
| Exposing low‑level operations to safe code | `Trust` capability |
| Normal application code | Neither — stay safe |

---

## 14. Data-Oriented Architecture: ECS & Scenes

### 14.1 Components

Components are plain‑data `component` structs (see §3.5) that serve as the building blocks for data‑oriented designs. They are copy‑by‑default, C‑compatible, and may contain pure transformation functions.

```zith
component Position {
    [x, y]: f32
}

component Velocity {
    dx: f32,
    dy: f32
}

component Health {
    hp: u32,
    max_hp: u32,

    fn takeDamage(self: lend, damage: u32) {
        self.hp = if self.hp > damage { self.hp - damage } else { 0 };
    }
}
```

### 14.2 Scenes (Memory Regions)

A Scene is a memory region (arena / zone allocator). Entering a new scene **replaces** the previous one — resetting memory rather than stacking. A Scene may specify a custom `Allocator`.

```zith
// Default allocator
scene GameLevel {
    let terrain  = @allocate(Terrain, levelData);
    let entities = @allocate([_]Entity, entityList);
    runLevel(terrain, entities);
    // all memory freed here
}

// Custom bump allocator
scene GameFrame: BumpAllocator {
    let scratch = @allocate([4096]u8);
    renderFrame(scratch);
}
```

```zith
scene MainMenu {
    entity MenuButton { /* ... */ }
    entity TextDisplay { /* ... */ }
}

scene GameLevel {
    entity Player { /* ... */ }
    entity Enemy { /* ... */ }
    entity Item { /* ... */ }
}

// Transition between scenes
transition(MainMenu, GameLevel);  // old scene dies, new scene allocates
```

> `@` — compiler intrinsics / reflection: `@allocate`, `@struct`, `@nullable`, etc.
> `#` — variable/field attributes: `#volatile`, `#thread_local`, `#wontRemain`, etc.

### 14.3 ECS Patterns

Use a **scene per logical region**:

```zith
scene MainLevel { /* all main level entities/resources */ }
scene BossRoom  { /* boss‑specific entities */ }
```

---

## 15. Macros

| Type | Description |
|---|---|
| Normal (scoped) | Hygienic. Symbols do not leak into the call site scope. **Requires `@` prefix at call site.** |
| Raw macro | Inserts code literally at the call site. Not hygienic. **Requires `@` prefix at call site.** |
| Tag macro | HTML‑like syntax. Tag attributes (e.g. `id=5`) available as `attributes` (only when macro is the first argument). Content between tags forms remaining arguments. **Uses `<>` syntax, no `@` prefix.** |

> Best practice: define macros inside a `context` block.

```zith
macro log(msg: expr) { @println("[LOG] ", msg); }

raw macro swap(a: identifier, b: identifier) {
    let _tmp = a; a = b; b = _tmp;
}

<Section title="Overview"> body </Section>

// Macro parameter meta‑types: identifier, expr, condition, body
```

### 15.1 `@` Prefix Rule

Normal and raw macros require the `@` prefix at the call site. This distinguishes them from function calls:

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

Tag macros use `<>` syntax and do not need `@`:

```zith
<div class="container"> content </div>
<Section title="Overview"> body </Section>
```

---

## 16. Words (Custom Operators)

Words are custom operators using identifiers. Positions: **prefix**, **infix**, **suffix**. Each position has a fixed language‑defined precedence.

- Must be activated with `use` even if already imported.
- Two words of the same name in the same scope: compile error.
- Compiler errors on any ambiguity, even potential ambiguity.
- Best practice: define words inside a `context`.

```zith
use math.vec.dot as DOT;
use math.vec.cross as CROSS;

let d = vec1 DOT vec2;
let c = vec1 CROSS vec2;
```

---

## 17. Contexts

A context bundles macros, constants, words, and other declarations. Applied to a block or activated globally (one active per category at a time).

```zith
use SQL QueryBlock {
    SELECT * FROM users WHERE id = :id
}

use SQL;   // global activation -- replaces previous context
```

All named blocks in Zith may receive a context.

---

## 18. C Interop

Zith has three modes of C interop, designed to make using C libraries as frictionless as possible.

### 18.1 Automatic Binding via `.h`

Including a C header automatically generates bindings. All functions become `raw fn` by default. Pointer types are inferred:

```zith
import "openssl/ssl.h";

// All C functions now available as raw fn
SSL_CTX_new(method);
```

| C type | Inferred Zith type |
|---|---|
| `T*` | `share T` (non‑owning by default) |
| `const T*` | `T*` |
| `T**` | `share T*` (assumes ownership) |
| `void*` | `raw opaque` |
| `int`, `float`, etc. | direct primitive equivalents |

### 18.2 Manual Binding with Semantic Annotation

Override or supplement auto‑generated bindings to add Zith semantics:

```zith
// Raw bind (default)
fn bindToC = extern 'C' malloc(size: u64): opaque;

// With semantic annotation -- compiler enforces qualifier on Zith side
fn bindToC = extern 'C' malloc(size: u64): unique opaque;
//                                          ^ compiler treats return as unique owned
```

### 18.3 External (No Header)

For assembly routines, headerless libraries, or code deliberately outside the project:

```zith
// Linker resolves; compiler has no information about the function
fn bindTo = extern someAsmRoutine(x: u64): u64;
```

### 18.4 Exposing Zith to C

```zith
extern 'C' fn my_function(x: i32): i32 {
    x * 2
}
// Generates a C‑compatible symbol; callable from C as a regular function
```

---

## 19. Project Configuration

### 19.1 `ZithProject` (per‑project)

```toml
[project]
name    = "my_app"
version = "0.1.0"

[build]
runtime = true          # default: true; set false for OS/embedded
asm     = "x86_64_intel"  # required if using inline assembly
                          # error if diverges from machine or other project files

[assets]
paths = ["assets/", ]  # compile‑time validated asset paths

[dependencies]
std = "bundled"
```

### 19.2 `ZithFlags` (compiler / global)

```
--runtime=false     # disable runtime globally: removes most allocators,
                    # disables `must`, disables dynamic linked libs and
                    # anything that depends on them, forces all available
                    # std to be statically linked
--asm=arm64         # set assembly dialect globally
--release           # release mode (raw warns, must -> UB)
--debug             # debug mode (default)
```

> `runtime = false` disables heap, standard stack assumptions, and signal handlers. All standard library features that require a runtime become unavailable at compile time.

---

## 20. Standard Library

### 20.1 Three‑Part Structure

**`std`** — Stable, essential APIs (guaranteed backward compatibility)

```zith
import std;
```

**`utils`** — Experimental, unstable features (may change)

```zith
import utils;  // use with caution
```

**`c`** — C ecosystem bindings (FFI)

```zith
import c;      // direct C library access
```

### 20.2 Common Modules

```zith
std/io/console{println, print, eprint}
std/collections{DynArray, HashMap, Set}
std/memory/malloc
std/fs/{File, open, read, write}
std/json
```

### 20.3 Traits

```zith
trait Copy { }
trait Shared { }
trait Lent { }
trait ToString { }
trait Drawable { }
```
> More will be added later.

---

## 21. Best Practices & Patterns

### 21.1 Ownership Patterns

- **Default to `unique`:** `let resource: unique = Resource.new();`
- **Use `share` for intentional multiple owners:** implement `Share` and clone.
- **Use `view` for reading:** `fn process(config: view Config)`
- **Use `lend` for temporary mutation:** `fn update(state: lend GameState)`
- **Use `belong` for part‑of relationships** (back‑pointers).

### 21.2 Optional & Failable Patterns

- **Prefer `?...or` for optionals:** `let name = ?user.name or "guest";`
- **Prefer `!...or` for fallaibles:** `let config = !loadPrimary() or loadBackup() or defaultConfig();`
- **Avoid `must` except for initialization:** `const API_KEY = must !env("API_KEY");`

### 21.3 Context Patterns

- **Group related DSL features:** define macros and words inside a `context` block.
- **Activate one global context per domain** to avoid pollution.

### 21.4 Error Handling Patterns

- **Add context to errors** with `fail` blocks and custom `throw`.
- **Use error chains:** `let data = !step1() or step2() or step3() or AllFailed;`

### 21.5 Macro Patterns

- **Prefer scoped macros inside contexts** rather than global activation.
- Use the `@` prefix for macros, bare names for functions to keep code clear.

### 21.6 Naming Conventions

| Construct | Convention | Examples |
|---|---|---|
| Variables & functions | camelCase | `playerHealth`, `getDamage`, `loadConfig` |
| Components | single word, lowercase | `rgb`, `color`, `file`, `vertex`, `health` |
| Structs | PascalCase | `Point`, `Container`, `DynArray`, `GameConfig` |
| Traits & interfaces | PascalCase | `Printable`, `iPositioned` |
| Files | kebab-case | `game-loop.zith`, `asset-manager.zith` |
| Constants & comptime | UPPER_SNAKE_CASE | `MAX_SIZE`, `PI`, `DEFAULT_TIMEOUT` |
| Enums | PascalCase for type, PascalCase or UPPER for variants | `enum Direction { North, South }` |

---

## 22. Appendix — Keyword & Operator Reference

| Keyword | Category | Summary |
|---|---|---|
| `import` / `from` / `export` | Module | Import / inject into scope / re‑export. |
| `alias` | Module | Name alias for a type, namespace, or symbol. |
| `use` | Module | Bring a word, context, or operator into scope. |
| `type` | Types | Distinct type copy or compile‑time constraint (with `or`). |
| `as` | Types | Cast / coercion. Also used in `implement T as Trait`. |
| `is` | Types | Type‑check / narrowing. Boolean. Supports `@struct`, `@nullable`, etc. |
| `enum` | Types | Closed compile‑time constants. C‑style, struct‑backed, or ADT‑style. |
| `union` | Types | Runtime tagged union. Variants separated by commas. |
| `struct` | Types | Record type. Fields may be grouped with `[]`. |
| `component` | Types | POD / Copy‑by‑default struct. No traits. C‑compatible. |
| `implement` | Types | `implement T {}` or `implement T as Trait {}`. |
| `when` | Types | Pattern matching. Ranges, type dispatch, branch tags, `_(N)` ignore. |
| `[]T` / `[N]T` / `[_]T` | Types | Slice / fixed array / deduced‑size array. |
| `\| \|` | Types | Pack — named tuple / variadic / closure capture group. |
| `pub` / `mod` / `mod(..)` / `mod(N)` | Visibility | Public / module‑local with optional depth. |
| `let` / `var` / `global` / `const` / `comptime` | Bindings | Immutable / mutable / static / compile‑time / compile‑time mutable. |
| `default` / `lend` / `view` / `unique` / `share` / `belong` | Memory | NRA memory qualifiers. See §7. |
| `fn` / `const fn` / `async fn` / `async<Generator> fn` / `flow fn` / `raw fn` | Functions | Function kinds. Orthogonal; cannot be mixed. |
| `yield` | Functions | Suspend an async fn; optionally produce a value. |
| `trait` / `interface` / `capability` / `extends` / `requires` / `dyn` | OOP | Nominal traits, structural interfaces, capabilities, extension, constraints, dynamic dispatch. |
| `Copy` / `Functor` / `Arithmetic` / `Error` | Capabilities | Operator and behaviour capabilities. |
| `Null` / `Fail` | Capabilities | Negative — activate only in proven‑invalid states. |
| `Allocator` / `Generator` / `Share` / `Lent` / `Trust` / `Unique` | Capabilities | Memory, async, threading, and safety capabilities. |
| `marker` / `dock` / `jump` | Flow | Hoisted blocks, jump sites, invocations for `flow fn`. |
| `stackful` | Flow | Opt‑in modifier for stackful markers (default is stackless). |
| `->` / `..` | Chain | Chain flow / placeholder for previous value. Left‑to‑right. |
| `,` (in chain) | Chain | Sub‑chain: applies but does not advance the main chain value. |
| `spawn` / `await` / `handle.send` | Threads | Spawn thread, wait, send message. |
| `#wontRemain` | Threads | Promise: thread dies before enclosing scope ends. |
| `?T` / `T!` | Errors | Optional / Result types. May be stacked. |
| `?` / `!` (postfix) | Errors | Propagate Option / Result. No semicolon. Propagate out of chains. |
| `or` | Errors / Loops / Types | Fallback / collapse optional loop return / type constraint separator. |
| `must` | Errors | Panic in debug; UB in release if invalid. |
| `raw` | Errors / Raw | Always unchecked (debug + release). Compiler warns in release. |
| `unsafe` | Raw | Stronger bypass than raw. Only valid inside raw contexts. |
| `throw` / `fail` / `continue(v)` / `with` / `with!` | Errors | Explicit throw, scoped recovery, resume, bundled fallible ops. |
| `::` | Operators | Scope resolution — access shadowed outer name. |
| `and` / `or` / `not` / `xor` | Operators | Logical (English keywords). |
| `&` / `\|` / `^` / `~` / `<<` / `>>` | Operators | Bitwise (standard symbols). |
| `@` / `#` | Annotations | `@` intrinsics/reflection + macro prefix. `#` variable/field attributes. |
| `extern 'C'` | Interop | C binding — auto via `.h`, manual, or external. |
| `runtime` / `asm` | Config | `ZithProject` / `ZithFlags` build settings. |
| `assets` | Config | `ZithProject.toml` asset path declarations. |

## Compiler Intrinsics & Attributes

| Intrinsic | Summary |
|---|---|
| `@fields(T)` | Iterate fields/members of a type |
| `@sizeOf(T)` | Size of type in bytes |
| `@hasTrait(T, Trait)` | Check if type implements a trait |
| `@struct` / `@component` / `@union` / `@enum` | Type kind checks (used with `is`) |
| `@nullable` | Check if type is nullable (`?T`) |
| `@primitive` | Check if type is a primitive |
| `@allocate(T, data)` | Scene‑scoped allocation |
| `@pack` | Extract/inject closure pack |
| `@toStruct(entity)` | Convert pack to plain struct |
| `@toPack(struct, scene)` | Convert struct back to pack |
| `@appendField Type, name: T` | Add field to a type being constructed |
| `@appendMethod Type, fn ...` | Add method to a type being constructed |
| `@file()` / `@line()` / `@fnName()` | Location information |
| `@location()` | Rich panic message source |

| Attribute | Summary |
|---|---|
| `#volatile` | Variable is volatile (compiler must not optimise away) |
| `#locaL_worker` | Variable is thread‑local storage |
| `#wontRemain` | Promise: thread dies before enclosing scope ends |

---

*Zith Language Specification — Draft v0.9 — Subject to change*