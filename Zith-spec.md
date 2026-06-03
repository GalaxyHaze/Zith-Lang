# ZITH Language Specification
*Draft v0.8 — 2026*

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
11. [Macros](#11-macros)
12. [Words (Custom Operators)](#12-words-custom-operators)
13. [Contexts](#13-contexts)
14. [Scenes (Memory Regions)](#14-scenes-memory-regions)
15. [C Interop](#15-c-interop)
16. [Project Configuration](#16-project-configuration)
17. [Full Example](#17-full-example)
18. [Appendix — Keyword & Operator Reference](#18-appendix--keyword--operator-reference)

---

## 1. Overview & Design Philosophy

### 1.1 What Zith Is

Zith is a statically typed, compiled, general-purpose language. Its primary paradigm is OOP built around traits and interfaces rather than classical inheritance. Functional and procedural styles are first-class citizens.

### 1.2 The Toolbox Philosophy

Zith has a deliberately small, stable core and a large, specialised toolbox.

| Core (always used) | Specialised toolbox (domain-specific) |
|---|---|
| `struct`, `fn`, `lend`, `view` | `scene` — for games / memory-intensive domains |
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

---

## 2. Module System

### 2.1 Import Keywords

| Keyword | Behaviour |
|---|---|
| `import path as name` | Imports a module; path becomes its namespace. Access via `name.symbol`. |
| `from path` | Injects all public symbols directly into scope (sugar). |
| `export path` | Re-exports a dependency; consumers of this module also receive it. |

```zith
import std/io/console as console;
console.println("hi");

from std/io/console;
println("hi");

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

use DOT = math.vec.dot;
use SQL;
```

### 2.3 Namespace Access & Scope Resolution

Namespaces accessed with `.` — e.g. `std.io.console.println`. The `::` operator reaches upward past a shadowed name to the outer scope.

```zith
let x = 10;
{
    let x = 20;
    println(::x);   // 10 -- outer scope
}
```

### 2.4 Type Constraints vs Union Separators

| Construct | Separator | Semantics |
|---|---|---|
| Type constraint | `\|` (pipe) | Compile-time template restriction. Dispatched at compile time. |
| Union body | `,` (comma) | Runtime tagged union. Variants separated by commas. |

```zith
// Type constraint -- compile-time dispatch
type Number = i32 | f64 | bool;
fn convert<T: Number>(val: T): string { ... }

// Union -- runtime tagged
union Number { i32, f64, bool }
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
| Compiler-internal | `unknown` — valid but unresolved type (not user-instantiable). `invalid` — dead/uninitialised state (moved variable, proven-null). Neither can be named or stored by user code. |
| Special | `noreturn`, `null` |
| Opaque | `opaque` — non-owning tagged void\*. `unique opaque` — owns the pointed-to value. `raw opaque` — untagged void\* (C interop). |

### 3.2 Slice & Array Types

| Syntax | Meaning |
|---|---|
| `[]T` | Slice — fat pointer (pointer + length). String literals are `[]char`. |
| `[N]T` | Fixed-size array, stack-allocated. |
| `[_]T` | Deduced-size array — compiler infers N from the initialiser. |

#### Strings & Origin Tracking

NRA tracks the **origin** of every string node (`literal`, `allocator`, `local`, `view`):

```zith
// []char implicitly casts to string -- zero cost (literal origin)
let s: string = "hello";

// Concatenation changes origin to allocator -- triggers allocation
let greeting = "hello" + " " + "world";
```

> `string` is a built-in library type. `[]char` is the primitive representation.

### 3.3 Enum

A closed set of named constants. All values must be known at compile time. Three styles:

#### C-style
```zith
enum Direction { North, South, East, West }
enum Status: i32 { Ok = 0, Err = 1, Pending = 2 }
```

#### Struct-backed
```zith
enum Color: rgb {
    red   = { r: 255, g: 50,  b: 0,   a: 255 },
    green = { 0, 255, 0, 255 },
}
```

#### ADT-style (Rust-like)
```zith
union Shape {
    Circle = { radius: f32 },
    Rect   = { w: f32, h: f32 },
    Point,
}

enum Constants: union{
    pi = 3.14f,
    vector = |x: -1, y: 0, z: -1, w: 1|,
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

#### Generic Structs & Self-referential Patterns

```zith
struct Pair<T, U> { first: T, second: U }

// Doubly-linked list node
struct Node<T> {
    data: T,
    next: ?unique Self,   // owns next; null at tail
    prev: ?belong Self,   // back-ref; lifetime tied to parent; null at head
}
```

#### Implementation Blocks

```zith
implement Node<T> {
    fn isHead(self): bool { self.prev is null }
    fn isTail(self): bool { self.next is null }
}

implement Node<T> as Printable {
    fn print(self) { println("Node({self.data})"); }
}
```

> A struct body may declare method signatures without bodies. Those must then be implemented in an `implement` block.

#### `self`, `other`, `Self`

`self` is the current instance. `other` is a conventional second instance parameter. `Self` (capitalised) refers to the concrete type being implemented.

### 3.5 Component

A plain-old-data (POD) struct. **Copy by default** (alongside primitives). Cannot implement traits. May have inline functions limited to pure transformations. Represents a C-compatible struct.

```zith
component Vec2 {
    [x, y]: f32,
    fn length(self): f32 { sqrt(self.x*self.x + self.y*self.y) }
    fn dot(self, other: Self): f32 { self.x*other.x + self.y*other.y }
}
```

A component must satisfy all of the following (compiler-enforced):
- All fields are primitives or other components (no structs, unions)
- No trait implementations (no `implement ComponentName as Trait`)
- Inline functions may only perform pure transformations:
  - No side effects (I/O, allocation, global mutation)
  - Only arithmetic, comparisons, and field access
  - Must return a value; cannot return void
- Copy is always bitwise (memcpy-safe)
- Layout is C-compatible (no vtable, no fat pointers)
- Cannot contain `self`-referential fields (?unique Self, ?belong Self)

### 3.6 Union

By default an ```union``` is runtime tagged. Variants separated by commas.

```zith
union Value { i32, f64, bool }

// Type hint forces union deduction
let x: union = when (flag) { 0 = 42, 1 = 3.14, 2 = true };
```

> Without the `union` type hint the compiler cannot deduce a union — it must be stated explicitly.

```raw union``` is a C-union, can only be used on ```raw``` context
> Accessing the wrong type is UB 

### 3.7 Packs (`| |`)

A **Pack** unifies variadics, named tuples, and explicit closure captures. A closure IS(internally) a pack plus a static fn or a fn ptr

```zith
// Bare pack -- named tuple
let pos  = |x: 1.0, y: 2.0, z: 0.0|;
println(pos.x);       // named access
println(pos.0);       // positional access

// Closure = pack + () + body
//by default is move
let addBase = |view base| (n: i32): i32 { base + n };

// Explicit capture, move the 'base' to the closure
let f = |base| (n: i32): i32 { base + n };
```

**```static``` vs ```dyn```**
A closure by default, do an **inline** fn call, while
```zith
let closure: dyn = |base|{...};
```
Makes an **indirect** fn call
>note: the 'pack' signature is still knowed at compile-time, to manipulate it.
```zith
//extracting the pack
let pack = @pack closure;
//inserting the pack again
@pack closure = pack;
```
>note: in order to make the pack signature runtime
```zith
//capturing 'opaque' introduces Type erasure
//here, we also use 'type hints' ( aka: partial type specialization )
let closure: |opaque| = (): i32 {...};
```

### 3.8 Union Narrowing (`is`) & Flow Typing

The `is` operator narrows a union within a branch. If the value is mutated inside the branch, the type **widens back** to the full union after the block.

```zith
fn handle(v: Val): void {
    if (v is i32) {
        println("int: {v}");    // v is i32 here
    } else (v is f64) {
        println("float: {v}");
    } else {
        println("str: {v}");    // compiler knows v is []char here
    }
    // v is Val again (full union) here
}

// when with branch tags
when (v) {
    n: i32  = println("int: {n}"),
    f64     = println("float: ", v),
    _       = println("other"),
}

//_ skips one, _(N) skips N
when (shape) {
    Circle = println("circle r=", shape.radius),
    Rect   = let [val, _] = shape,   // ignore second field
    _      = println("other"),
}

// Standalone boolean & compile-time reflection
// All conditons / booleans tests must be inside '()'
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
    0       = println("none"),
    1       = println("one"),
    2..10   = println("few"),
    _       = println("many"),
}

// As expression
let label = when (score) { 90..100 = "A",  70..90 = "B",  _ = "C" };

// .. ignore the rest
when (point) {
    [x, ..] = println("x=", x),
    _         = println("no match"),
}

// .. get the last element
when (point) {
    [.., w] = println("w={w}"),
    _         = println("no match"),
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
| Typing | Nominal — must be explicitly implemented. | Structural (duck-typed) — auto-satisfied if members match. |
| Can extend? | Yes (`requires` / `extends`). | No. |
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
    fn to_json(self): string;
}

// Disambiguate overlapping method names
Printable.print(self);
JsonSerializable.print(self);
```

### 4.3 Interfaces

Interfaces are structural: a type satisfies an interface automatically if it has the required members. A trait that `requires` an interface gains read/write access to those fields.

```zith
interface iPositioned { [x, y, z] f32 }

requires iPositioned
trait Movable {
    fn translate(self, dx: f32, dy: f32, dz: f32) {
        self.x += dx; self.y += dy; self.z += dz;
    }
}

// Any struct with x, y, z: f32 satisfies iPositioned automatically
struct Enemy { [x, y, z]: f32, health: i32 }
```

### 4.4 Capabilities — Built-in Reference

Capabilities are special traits consumed by the compiler to unlock operations, enforce rules, or modify NRA behaviour.

| Capability | What it enables |
|---|---|
| `Copy` | Implicit bitwise copy. Components and primitives are Copy by default. |
| `Functor` | `operator()` — makes the type callable like a function. |
| `Arithmetic` | Operators `+`, `-`, `*`, `/`, `%`, etc. |
| `Error` | `operator throw` — required for `throw MyError;` |
| `Null` | Activates associated traits only when NRA has proven the value IS null. Negative capability — fires on proven-invalid state. |
| `Fail` | Activates associated traits only when NRA has proven the value IS an error/broken. Cannot coexist with `Null` on the same value. |
| `Allocator` | Custom allocator backend for Scenes and `@allocate`. `delete fn free;` makes a bump-only allocator. |
| `Generator` | Enables `async<Generator> fn`. Only meaningful on async functions. |
| `Share` | Required for `global: share` and thread-crossing. **share is mutable** — does not imply read-only. |
| `Lent` | Enables `global: unique` runtime-checked exclusive borrow. |
| `Trust` | A trait extending `Trust` may contain `raw fn` methods callable from safe contexts. |
| `Unique` | Singleton type. Cannot be instantiated; type name acts as the instance. All fields implicitly global; mutable fields require thread-safety. |

#### Null & Fail — Negative Capabilities

```zith
implement ?Config as Null {
    fn on_missing(self) { log("Config was null -- using defaults"); }
}
implement Config! as Fail {
    fn on_error(self) { log("Config load failed"); }
}

let cfg: ?Config = load_config();
if (cfg is null) {
    cfg.on_missing();   // Null trait unlocked here
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
struct AppConfig { host: string, port: u16 }
implement AppConfig as Unique {}

AppConfig.host = "localhost";
AppConfig.port = 8080;
// let cfg = AppConfig { ... };  -- COMPILE ERROR
```

#### Share — Thread-crossing Mutable State

```zith
// Share is mutable -- multiple names, multiple threads, all can write
global: share counter: AtomicI32 = 0;

// Without Share capability, cross-thread access is a compile error
struct LocalOnly { data: i32 }
// global: share bad: LocalOnly = ...;  -- COMPILE ERROR: lacks Share
```

### 4.5 Operator Overloading

Capability-based and strict:

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

// Traits may extend capabilities
trait SafeBuffer extends Trust {
    raw fn read_byte(self, offset: u64): u8 {}
}
```

---

## 5. Functions

### 5.1 Return Type & Implicit Returns

```zith
fn add(a: i32, b: i32): i32 { a + b }   // explicit, implicit return
fn add(a: i32, b: i32)      { a + b }   // inferred type

fn first<T>(slice: []T): ?T {
    if (slice.len == 0) { null }
    else { slice[0] }
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

### 5.3 `async fn` & `yield`

```zith
// Default coroutine
async fn fetch(url: string): Response! {
    yield;
    get_response()!
}

// Explicit generator
async<Generator> fn range(start: i32, end: i32): i32 {
    let i = start;
    for (i < end) { yield i;  i += 1; }
}

for (v in range(0, 10)) { println(v); }
```

### 5.4 Closures & Packs

A closure is a pack plus an optional parameter list and body. The capture pack and parameter list are distinct:

```zith
// |capture pack| (parameters) { body }
let add_base = |base| (n: i32): i32 { base + n };

// Explicit capture modes (default: move)
let f1 = |view x| (n: i32) { x + n };
let f2 = |lend x| (n: i32) { x += n; };
```

| Level | Syntax | Semantics |
|---|---|---|
| Full static | `let f = \|x\| x + 1;` | Zero overhead. |
| Semi-dynamic | `let f: dyn = \|x\| x + 1;` | Function pointer. Capture signature statically known. |
| Dynamic pack, static fn | `let f: \|opaque\| = \|x\| x + 1;` | Capture pack opaque. fn pointer call overhead only. |
| Full dynamic | `let f: dyn \|opaque\| = \|x\| x + 1;` | Both function and capture opaque. Full fat pointer. |

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
| `const` | Both | Compile-time constant. Binding and content frozen. |
| `comptime` | Both | Mutable during compilation (e.g. compile-time counter), frozen at runtime. |

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

// In a for loop
let r = for ([acc, i]: i32), (i in 0..n) { acc *= i + 1 } or 0;
```

---

## 7. Memory Model (NRA)

### 7.1 What NRA Does (and Does Not Do)

Node Resource Analysis (NRA) is the final compiler pass, after SEMA + SOLVE have fully resolved all types. It is responsible for:

- Tracking whether a node (value) is `alive`, `dead`, or `lent`.
- Ensuring ownership rules: `unique` nodes have exactly one owner; `belong` nodes cannot outlive their parent.
- Validating aliasing: `share` allows multiple names; `lend` is exclusive.
- Tracking node origins (`literal`, `allocator`, `local`, `view`) to enable zero-cost coercions.

NRA does **not** infer types — that is SEMA/SOLVE's job.

### 7.2 Move Semantics

Moving `a` to `b` redirects the name `b` to `a`'s node. The name `a` is set to **dead** and cannot be read — only reassigned:

```zith
let a = Point { x: 1.0, y: 2.0 };
let b = a;                          // b -> a's node; a becomes dead
// println(a.x);                    -- COMPILE ERROR: a is dead
println(b.x);                       // OK

a = Point { x: 3.0, y: 4.0 };      // OK: reassignment creates new node for a
```

Effectively, if a is not reassigned ( since is let ), a never existed
and b has 'always' holding ```Point{x: i.0, y: 2.0}```

### 7.3 Memory Keywords

| Keyword | Relationship | Common use |
|---|---|---|
| `default` | Owned. Lifetime follows the binding. | Variables, struct fields |
| `lend` | Exclusive mutable temporary. Cannot be stored, moved, or captured. **Can be returned** (passes the behavioural promise to the caller). | Passing mutable refs to fns |
| `view` | Read-only non-owning ref. Many views may coexist. | Inspecting without ownership |
| `unique` | Single-owner guarantee. Only one name in the graph. | Ownership-transfer patterns |
| `share` | Multiple names, same node. Statically validated — no ref-count. Mutable. | Compile-time-proven sharing |
| `belong` | Part-of. Node lifetime tied to parent. Cannot be stored independently. | Back-pointers, hierarchies |

> In practice, most code only needs `lend` and `view`.

### 7.4 The Four NRA Rules

**Rule 1 — Argument Exclusivity:** in any call expression, each argument must refer to a distinct node. No exceptions.
- `default` / `unique` / `lend` duplicated → **ownership error**
- `share` / `view` duplicated → **logic error** (same resource in two argument positions is almost certainly a bug)

**Rule 2 — No Dead Node Access:** a symbol may not be read if its node is in state `dead`.

**Rule 3 — No Escaping `belong`:** a `belong` node may not be stored in any location whose lifetime exceeds any node in its `belong_set`. At every use, all parents must be `alive`.

**Rule 4 — `lend` Behavioural Promise:** a `lend` value may not be stored, moved, or captured. It may be passed as a call argument or returned (passing the promise to the caller).

### 7.5 NRA in Practice

```zith
// lend -- exclusive temporary borrow
fn scale(p: lend Point, factor: f32) { p.x *= factor; p.y *= factor; }
let mut pt = Point { x: 3.0, y: 4.0 };
scale(pt, 2.0);
println(pt.x);   // OK: borrow ended

// view -- multiple read-only refs
let v1: view Point = pt;
let v2: view Point = pt;   // fine

// share -- no ref-count, statically proven
let a: share Config = load();
let b: share Config = a;   // both point to same node

// belong -- back-pointer cannot outlive parent
struct Tree<T> {
    data:     T,
    children: []unique Self,
    parent:   ?belong Self,
}
```

### 7.6 NRA Limitations & Escape Hatches

NRA cannot statically validate all shared cycles across threads:

```zith
// await -- safe: compiler knows thread is done before scope ends
let handle = spawn worker(shared_data);
await handle;

// #wontRemain -- promise: thread dies before scope ends
#wontRemain let _ = spawn quick_task(shared_data);

// Rc -- runtime ref-count fallback
let shared = Rc.new(HeavyResource.init());
let _ = spawn worker(Rc.clone(shared));
```

### 7.7 Self-referential Types

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
```

---

## 8. Error Handling

Fully static, return-based. No exceptions. No semicolon required after `?` or `!`.

### 8.1 Failable Types

| Syntax | Meaning | Propagated by |
|---|---|---|
| `?T` | Optional — T or null. | `?` (postfix) |
| `T!` | Result — T or an error. Compiler infers anonymous error union for multiple error types. | `!` (postfix) |

Failable types may be stacked — notation is linear and unambiguous:

```zith
// Optional pointer to an optional Result of optional i32 or IoError
?*?(?i32 | IoError)
```

### 8.2 `must` vs `raw`

| | Debug mode | Release mode |
|---|---|---|
| `must` | Panics with file + line location. | Undefined behaviour (UB). Compiler may optimise assuming valid. |
| `raw` | Always unchecked. | Always unchecked. Compiler warns if present in release builds. |

```zith
let cfg: ?Config = try_load();
let c1 = must cfg;   // panics debug; UB release if null
let c2 = raw cfg;    // always unchecked; compiler warns in release
```

### 8.3 Propagation & Fallback

```zith
fn read_config(path: string): Config! {
    let file = File.open(path)!
    let data = file.read()!
    parse(data)!
}

let name = user.name? or "guest"
let data = primary()! or backup()! or default_data

// Propagation inside chain flow
readFile("data.bin") -> parse(..)! -> validate(..)? -> process(..)
```

### 8.4 `with` / `catch`

```zith
// Short-circuit: first failure jumps to catch
with (connectDb(), user: getUser(id)) {
    process(user);
} catch (err) { log(err); }

// Eager: all expressions evaluated; catch runs if any failed
with! (fetchA(), b: fetchB()) { use(b); }
catch (err) { log(err); }
```

### 8.5 `fail` Blocks

A `fail` block runs when an error would escape the associated scope. Can appear after a named block (external) or inside a block (nameless scope guard):

```zith
// External fail
loadConfigure {
    let raw = read_file("config.json")!
    parse(raw)!
} fail loadConfigure(err) {
    if (err == NotFound) { continue(default_config); }
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

Parentheses `()` are mandatory on all control structure conditions except function calls. Logical operators use English keywords; bitwise use standard symbols.

```zith
if (x > 0 and y < 10) { ... }
if isTrue() and (x > 5) { ... }
let mask = a & b | c ^ d;
```

### 9.2 `for`

```zith
for { ... }                                     // infinite
for (i in 0..=9) { println(i); }               // inclusive range
for (i in 0..9)  { println(i); }               // exclusive range
for (i = 0), (i < 10), (i += 1) { ... }       // init / cond / step
for (v in range(0, 100)) { println(v); }       // over a generator

// Destructure group with fallback
let r = for ([acc, i]: i32), (i in 0..n) { acc *= i + 1 } or 0;
```

> If the loop may not run, the return is deduced as optional — unless `or` collapses it to non-optional.

### 9.3 Chain Flow (`->`)

The `->` operator pipes output left-to-right. Previous value is `..`. Tags capture values for later. `!` and `?` propagate out of the chain normally. Precedence: left-to-right, lower than function calls.

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

// Comma sub-chain -- f1, f2 receive foo's value but do NOT advance the chain
foo(), f1(..), f2(..) -> f3(..);

// Parenthesised real sub-chain
foo(), (f1(..) -> f2()) -> f3(..);
```

> Comma sub-chains are useful for side-effects (logging, validation) without disrupting the main data flow.

### 9.4 `flow` Functions & Markers

```zith
flow fn run(data: Stream): void {
    marker Process(chunk: Chunk, count: i32) {
        transform(chunk);
        // count preserved from last invocation if not updated
        // markers CANNOT invalidate external unique/default nodes
        // can only view, lend, or copy from external scope
    }
    let i = 0;
    for item in data {
        dock { jump Process(item, i); }
        i += 1;
    }
}

// Global marker
marker ContextSwitch(next: TaskId) {
    save_registers();
    load_task(next);
}

// noreturn: dock return variable never modified; calling dock can never resume
flow fn scheduler(): noreturn { ... }
```

**Marker rules:**
- Hoisted to top of flow fn; skipped during normal execution.
- Cannot access external variables or declare their own locals.
- Arguments live in a `thread_local` blob. Bits preserved across invocations unless explicitly updated.
- May `view`, `lend`, or `copy` from external scope. May **not** invalidate external `unique` / `default` nodes.
- Global markers may call regular functions, but NOT `async` or `flow`-controlled functions — unless marked `noreturn`.

#### Stackful Markers (opt-in)

Markers may optionally have their own stack. When a `jump` is executed from inside a stackful marker, all local owned nodes are dropped (cleaned up) before the jump. Only owned (move/copy) or `view`/`lend` values may cross a `jump` — `lend` of a local is forbidden as it would not survive the cleanup.

---

## 10. Concurrency & Threads

### 10.1 Spawning

```zith
// Default thread type
let handle = spawn worker_fn(data);

// Explicit thread type capability
let handle = spawn GreenThread fn worker_fn(data);

// Fire-and-forget
let _ = spawn background_task();

// #wontRemain -- promise the thread dies before scope ends
#wontRemain let _ = spawn quick_task(shared_data);
```

### 10.2 Compile-time Safety Enforcement

If a thread accesses shared data, the compiler requires **one of**:
- `await handle` before the shared data goes out of scope.
- `#wontRemain` attribute on the spawn.

Missing both is a **compile error**. Violating the `#wontRemain` promise is not caught — the shared data may become a dangling reference.

| Keyword / Method | Semantics |
|---|---|
| `await handle` | Waits for the thread to finish. |
| `await global_var` | Blocks until the global variable has been initialised. |
| `handle.send(msg)` | Sends a message to the running thread. |
| `#wontRemain` | Attribute on spawn. Promise: thread dies before enclosing scope ends. |

```zith
let handle = spawn worker(shared_data);
await handle;

global cfg: Config;
await cfg;
println(cfg.host);
```

---

## 11. Macros

| Type | Description |
|---|---|
| Normal (scoped) | Hygienic. Symbols do not leak into the call site scope. |
| Raw macro | Inserts code literally at the call site. Not hygienic. |
| Tag macro | HTML-like syntax. Tag attributes (e.g. `id=5`) available as `attributes` (only when macro is the first argument). Content between tags forms remaining arguments. |

> Best practice: define macros inside a `context` block.

```zith
macro log(msg: expr) { println("[LOG] ", msg); }

raw macro swap(a: identifier, b: identifier) {
    let _tmp = a; a = b; b = _tmp;
}

<Section title="Overview"> body </Section>

// Macro parameter meta-types: identifier, expr, condition, body
```

---

## 12. Words (Custom Operators)

Words are custom operators using identifiers. Positions: **prefix**, **infix**, **suffix**. Each position has a fixed language-defined precedence.

- Must be activated with `use` even if already imported.
- Two words of the same name in the same scope: compile error.
- Compiler errors on any ambiguity, even potential ambiguity.
- Best practice: define words inside a `context`.

```zith
use DOT   = math.vec.dot;
use CROSS = math.vec.cross;

let d = vec1 DOT vec2;
let c = vec1 CROSS vec2;
```

---

## 13. Contexts

A context bundles macros, constants, words, and other declarations. Applied to a block or activated globally (one active per category at a time).

```zith
use SQL QueryBlock {
    SELECT * FROM users WHERE id = :id
}

use SQL;   // global activation -- replaces previous active context of same name
```

- Scoped to the block where declared unless marked `global`.
- Global context active until end-of-file or replaced by another of the same name.
- All named blocks in Zith may receive a context.

---

## 14. Scenes (Memory Regions)

A Scene is a memory region (arena / zone allocator). Entering a new scene **replaces** the previous one — resetting memory rather than stacking. A Scene may specify a custom `Allocator`.

```zith
// Default allocator
scene GameLevel {
    let terrain  = @allocate(Terrain, level_data);
    let entities = @allocate([_]Entity, entity_list);
    run_level(terrain, entities);
    // all memory freed here
}

// Custom bump allocator
scene GameFrame with BumpAllocator {
    let scratch = @allocate([4096]u8);
    render_frame(scratch);
}
```

> `@` — compiler intrinsics / reflection: `@allocate`, `@struct`, `@nullable`, etc.
> `#` — variable/field attributes: `#volatile`, `#thread_local`, `#wontRemain`, etc.

---

## 15. C Interop

Zith has three modes of C interop, designed to make using C libraries as frictionless as possible.

### 15.1 Automatic Binding via `.h`

Including a C header automatically generates bindings. All functions become `raw fn` by default. Pointer types are inferred:

```zith
import "openssl/ssl.h";

// All C functions now available as raw fn
SSL_CTX_new(method);
```

| C type | Inferred Zith type |
|---|---|
| `T*` | `opaque` (non-owning by default) |
| `const T*` | `view opaque` |
| `T**` | `unique opaque` (assumes ownership) |
| `void*` | `raw opaque` |
| `int`, `float`, etc. | direct primitive equivalents |
| nullable return | `?T` automatically |

### 15.2 Manual Binding with Semantic Annotation

Override or supplement auto-generated bindings to add Zith semantics:

```zith
// Raw bind (default)
fn bindToC = extern 'C' malloc(size: u64): opaque;

// With semantic annotation -- compiler enforces qualifier on Zith side
fn bindToC = extern 'C' malloc(size: u64): unique opaque;
//                                          ^ compiler treats return as unique owned
```

### 15.3 External (No Header)

For assembly routines, headerless libraries, or code deliberately outside the project:

```zith
// Linker resolves; compiler has no information about the function
fn bindToC = extern 'C' some_asm_routine(x: u64): u64;
```

### 15.4 Exposing Zith to C

```zith
extern 'C' fn my_function(x: i32): i32 {
    x * 2
}
// Generates a C-compatible symbol; callable from C as a regular function
```

---

## 16. Project Configuration

### 16.1 `ZithProject` (per-project)

```toml
[project]
name    = "my_app"
version = "0.1.0"

[build]
runtime = true          # default: true; set false for OS/embedded
asm     = "x86_64_intel"  # required if using inline assembly
                          # error if diverges from machine or other project files

[dependencies]
std = "bundled"
```

### 16.2 `ZithFlags` (compiler / global)

```
--runtime=false     # disable runtime globally
--asm=arm64         # set assembly dialect globally
--release           # release mode (raw warns, must -> UB)
--debug             # debug mode (default)
```

> `runtime = false` disables heap, standard stack assumptions, and signal handlers. All standard library features that require a runtime become unavailable at compile time.

---

## 17. Full Example

```zith
from std/io/console;

fn main() { println("Hello, world!"); }
```

```zith
import std/io/fs;
import std/io/console as con;

interface iNamed { [name] string }

requires iNamed
trait Describable { fn describe(self): string; }

struct Config { name: string, host: string, port: u16 }
// Config satisfies iNamed automatically

implement Config as Describable {
    fn describe(self): string {
        "[\(self.name)] \(self.host):\(self.port)"
    }
}

struct Node<T> {
    data: T,
    next: ?unique Self,
    prev: ?belong Self,
}

fn load<T: Describable>(path: string): T! {
    let raw = fs.read(path)!
    parse(raw)!
}

fn run(): void {
    fs.read("app.cfg")
        -> raw:  parse::<Config>(..)
        -> cfg:  raw or Config { name: "default", host: "localhost", port: 8080 }
        -> {
            let tag = when (cfg.port) {
                80         = "http",
                443        = "https",
                8000..9000 = "dev",
                _          = "custom",
            };
            con.println("Using: ", cfg.describe(), " [", tag, "]");
        };
}
```

---

## 18. Appendix — Keyword & Operator Reference

| Keyword | Category | Summary |
|---|---|---|
| `import` / `from` / `export` | Module | Import / inject into scope / re-export. |
| `alias` | Module | Name alias for a type, namespace, or symbol. |
| `use` | Module | Bring a word, context, or operator into scope. |
| `type` | Types | Distinct type copy or compile-time constraint (with `\|`). |
| `as` | Types | Cast / coercion. Also used in `implement T as Trait`. |
| `is` | Types | Type-check / narrowing. Boolean. Supports `@struct`, `@nullable`, etc. |
| `enum` | Types | Closed compile-time constants. C-style, struct-backed, or ADT-style. |
| `union` | Types | Runtime tagged union. Variants separated by commas. |
| `struct` | Types | Record type. Fields may be grouped with `[]`. |
| `component` | Types | POD / Copy-by-default struct. No traits. C-compatible. |
| `implement` | Types | `implement T {}` or `implement T as Trait {}`. |
| `when` | Types | Pattern matching. Ranges, type dispatch, branch tags, `_(N)` ignore. |
| `[]T` / `[N]T` / `[_]T` | Types | Slice / fixed array / deduced-size array. |
| `\| \|` | Types | Pack — named tuple / variadic / closure capture group. |
| `pub` / `mod` / `mod(..)` / `mod(N)` | Visibility | Public / module-local with optional depth. |
| `let` / `var` / `global` / `const` / `comptime` | Bindings | Immutable / mutable / static / compile-time / compile-time mutable. |
| `default` / `lend` / `view` / `unique` / `share` / `belong` | Memory | NRA memory qualifiers. See §7. |
| `fn` / `const fn` / `async fn` / `async<Generator> fn` / `flow fn` / `raw fn` | Functions | Function kinds. Orthogonal; cannot be mixed. |
| `yield` | Functions | Suspend an async fn; optionally produce a value. |
| `trait` / `interface` / `capability` / `extends` / `requires` / `dyn` | OOP | Nominal traits, structural interfaces, capabilities, extension, constraints, dynamic dispatch. |
| `Copy` / `Functor` / `Arithmetic` / `Error` | Capabilities | Operator and behaviour capabilities. |
| `Null` / `Fail` | Capabilities | Negative — activate only in proven-invalid states. |
| `Allocator` / `Generator` / `Share` / `Lent` / `Trust` / `Unique` | Capabilities | Memory, async, threading, and safety capabilities. |
| `marker` / `dock` / `jump` | Flow | Hoisted blocks, jump sites, invocations for `flow fn`. |
| `->` / `..` | Chain | Chain flow / placeholder for previous value. Left-to-right. |
| `,` (in chain) | Chain | Sub-chain: applies but does not advance the main chain value. |
| `spawn` / `await` / `handle.send` | Threads | Spawn thread, wait, send message. |
| `#wontRemain` | Threads | Promise: thread dies before enclosing scope ends. |
| `?T` / `T!` | Errors | Optional / Result types. May be stacked. |
| `?` / `!` (postfix) | Errors | Propagate Option / Result. No semicolon. Propagate out of chains. |
| `or` | Errors / Loops | Fallback / collapse optional loop return. |
| `must` | Errors | Panic in debug; UB in release if invalid. |
| `raw` | Errors | Always unchecked (debug + release). Compiler warns in release. |
| `throw` / `fail` / `continue(v)` / `with` / `with!` | Errors | Explicit throw, scoped recovery, resume, bundled fallible ops. |
| `::` | Operators | Scope resolution — access shadowed outer name. |
| `and` / `or` / `not` / `xor` | Operators | Logical (English keywords). |
| `&` / `\|` / `^` / `~` / `<<` / `>>` | Operators | Bitwise (standard symbols). |
| `@` / `#` | Annotations | `@` intrinsics/reflection. `#` variable/field attributes. |
| `extern 'C'` | Interop | C binding — auto via `.h`, manual, or external. |
| `runtime` / `asm` | Config | `ZithProject` / `ZithFlags` build settings. |

## Compiller intrincs &  attributes

---

*Zith Language Specification — Draft v0.8 — Subject to change*
