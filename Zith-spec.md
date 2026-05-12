# Zith Language Specification
**Version:** 2.0 (Architectural Revision - Complete)  
**Status:** Unified & Comprehensive  
**Last Updated:** April 2026

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Design Philosophy](#2-design-philosophy)
3. [Node Resource Model (NRM)](#3-node-resource-model-nrm)
4. [Ownership Keywords (5 Rules)](#4-ownership-keywords)
5. [Type System](#5-type-system)
6. [Type Navigation (Optional & Failable)](#6-type-navigation)
7. [Lexical Structure & Operators](#7-lexical-structure--operators)
8. [Contexts (DSL Namespaces)](#8-contexts-dsl-namespaces)
9. [Packs (Tuples & Ownership)](#9-packs-tuples--ownership)
10. [Control Flow: Markers, Flows & Functions](#10-control-flow-markers-flows--functions)
11. [Data-Oriented Architecture: ECS & Scenes](#11-data-oriented-architecture-ecs--scenes)
12. [Intrinsics (@)](#12-intrinsics)
13. [Error Handling & Try/Catch](#13-error-handling--trycatch)
14. [Concurrency & Globals](#14-concurrency--globals)
15. [Standard Library](#15-standard-library)
16. [Zith vs Other Languages](#16-zith-vs-other-languages)
17. [Best Practices & Patterns](#17-best-practices--patterns)

---

## 1. Introduction

Zith is a **systems programming language** designed for the "Universal Developer" who builds everything: kernels, game engines, web backends, UI frameworks.

### 1.1 Core Principles

1. **Explicit over Implicit** — Every behavior visible in code
2. **Safety by Default** — Memory and type safety enforced at compile-time
3. **Zero-Cost Abstractions** — High-level code compiles to efficient machine code
4. **Universal Applicability** — One language, one compiler, infinite domains (via Contexts & ECS)

### 1.2 Why Zith?

| Goal | Zith Solution |
|------|---|
| **Memory Safety** | NRM + 5 Keywords verify memory validity at compile-time |
| **Complex Data** | `extension` keyword eliminates weak pointers, Pin, unsafe |
| **Data-Oriented** | Native ECS (Entities, Components, Scenes) |
| **DSLs** | Contexts + Tag Macros for Web, SQL, Graphics, etc. |
| **Simplicity** | Low learning curve; advanced features available when needed |

---

## 2. Design Philosophy

### 2.1 The "Toolbox" Approach

**Core Language:** Small, learnable, safe.
**Extended Language:** Powerful tools for complex problems.

### 2.2 Explicit Safety

- **Ownership:** 5 explicit keywords (not implicit reference counting)
- **Memory:** NRM tracks origin (stack/heap) and validity
- **Traits:** Capabilities declared at definition, not runtime

### 2.3 Structural vs Nominal Typing

- **Nominal:** Named types, stable APIs (structs, entities)
- **Structural:** Anonymous tuples, unions, inline types (quick prototyping)

Both seamlessly coexist in Zith.

---

## 3. Node Resource Model (NRM)

### 3.1 Conceptual Model

All data in Zith is a **Node** in a **Directed Graph**.

- **Nodes:** Values (instances, data)
- **Edges:** Variables (references, bindings)
- **Roots:** Scope (functions, blocks, scenes)

The compiler performs **Connectivity Analysis**:
```
Can I reach this Node from my current scope?
├─ Is there a valid path of edges?
├─ Is each edge valid (non-zombie)?
└─ Is the Node still allocated?
```

### 3.2 Memory Origins

NRM tracks **where memory comes from**:

- **Stack:** Automatic (scope-bound), fast, small
- **Heap:** Manual allocation, persistent, large
- **Arena/Allocator:** Custom allocation strategy (via scenes)

### 3.3 Validity Tracking

The compiler maintains **two states** for each node:

1. **Alive:** Node exists, can be accessed
2. **Zombie:** Node was moved/consumed; invalid in this branch

### 3.4 Lazy Checking & Branch Analysis

If a node is used in one branch but not another:

```zith
let data = getData();

if condition {
    use(data);           // data is consumed
    return void;
} else {
    return data;         // data is still alive here
}

// Compiler infers: ?data (Optional based on path)
```

The compiler **analyzes both branches** and generates a union type representing the possible states.

---

## 4. Ownership Keywords

### 4.1 Five Keywords, Five Rules

All keywords enforce at **compile-time** via NRM connectivity analysis.

#### `unique` — Exclusive Ownership

- **Rule:** Only one edge (reference) can point to this node at any time
- **Move Semantics:** When assigned, the original reference becomes zombie
- **Thread-Safe:** Yes (if combined with `Shared` trait)
- **Use Case:** Standard ownership, default for complex types

```zith
let resource = unique ResourceHandle.new();
let other = resource;              // resource is now zombie
other.use();                        // ✅
resource.use();                    // ❌ ERROR: zombie
```

#### `share` — Shared Ownership

- **Rule:** Multiple edges can point to the node simultaneously
- **Mutability:** All owners can mutate (no interior mutability needed)
- **Thread-Safe:** Requires `Shared` trait implementation
- **Use Case:** Multiple owners, shared mutable state

```zith
let data = Data.new();
let s1 = data as share;
let s2 = s1;                       // ✅ both valid
s1.mutate();                       // ✅
s2.read();                         // ✅
```

#### `view` — Read-Only Borrow

- **Rule:** No ownership transfer; read-only access
- **Mutability:** Immutable (cannot modify)
- **Thread-Safe:** Yes, intrinsically
- **Lifetime:** Borrow must not outlive the owner
- **Use Case:** Efficient reading without ownership

```zith
fn print_info(data: view Resource) {
    println!("{}", data.name);     // ✅ read
    data.modify();                 // ❌ ERROR: immutable
}

let res = Resource.new();
print_info(view res);              // ✅ res still valid after
```

#### `lend` — Mutable Borrow

- **Rule:** Exclusive mutable access; original owner cannot access until returned
- **Mutability:** Mutable
- **Thread-Safe:** Yes (exclusive access prevents races)
- **Lifetime:** Must be returned to original owner
- **Use Case:** Temporary mutable modification

```zith
fn increment(counter: lend i32) {
    counter += 1;
}

var x = 0;
increment(lend x);                 // ✅ x is borrowed
println!("{}", x);                 // prints 1, x is returned
```

#### `extension` — Hierarchical Part-Of

- **Rule:** Child is structurally part of parent; cannot outlive parent
- **Ownership:** Neither exclusive nor shared; hierarchical
- **Mutability:** Mutable (within parent context)
- **Thread-Safe:** Yes (tied to parent's safety)
- **Use Case:** Self-referential structures (trees, linked lists)
- **Benefit:** No weak pointers, no unsafe, no Pin

```zith
struct Node<T> {
    value: T,
    next: share Node<T>?,           // can have other owners
    prev: extension Node<T>?        // structurally part of parent
}

// No cyclic reference issues, no weak pointers needed
```

### 4.2 Ownership Summary Table

| Keyword | Owns | Exclusive | Mutable | Thread Safe | Cost |
|---------|------|-----------|---------|------------|------|
| `unique` | Yes | Yes | Yes | Yes† | Zero |
| `share` | Yes | No | Yes | Yes† | Zero |
| `view` | No | — | No | Yes | Zero |
| `lend` | No | Yes | Yes | Yes | Zero |
| `extension` | No | — | Yes | Yes | Zero |

*†: Requires `Shared` trait*

### 4.3 Keyword Rules (Applied to Nodes)

The NRM foundation remains the same for all keywords. The keywords are **rules applied on top**:

```
NRM (Foundation)
    ↓
Keyword Rules (Applied)
    ├─ unique: only one edge alive
    ├─ share: multiple edges alive
    ├─ view: read-only access
    ├─ lend: exclusive mutable borrow
    └─ extension: part-of relationship
```

---

## 5. Type System

### 5.1 Primitive Types

```zith
// Integers
i8, i16, i32, i64, i128, isize
u8, u16, u32, u64, u128, usize

// Floating Point
f32, f64

// Boolean & Character
bool, char

// Unit (void)
void
```

### 5.2 Type Modifiers

#### Optional (`?T`)

Represents a value that may or may not exist.

```zith
let maybe: ?i32 = 42;
let nothing: ?i32 = null;

// Internally: union { T, null }
```

#### Failable (`T!`)

Represents a computation that may fail.

```zith
fn divide(a: i32, b: i32): i32! {
    if b == 0
        throw DivisionByZero;
    a / b
}

// Internally: union { T, Error }
```

### 5.3 Composite Types

#### Structs

```zith
struct User {
    id: u64,
    name: []char,
    email: ?string,
    active: bool
}

let u = User {
    id: 123,
    name: "Alice",
    email: null,
    active: true
};
```

#### Tuples (Product Types)

```zith
let pair: (i32, f32) = (42, 3.14);
let named: (x: i32, y: i32) = (x: 10, y: 20);

// Access
let a = pair.0;
let b = named.x;
```

#### Arrays & Slices

```zith
let arr: [5]i32 = [1, 2, 3, 4, 5];      // fixed
let slice: []i32 = arr[1..4];            // dynamic
```

#### Enums (Nominal)

```zith
enum Color {
    Red,
    Green,
    Blue
}

enum Result<T, E> {
    Ok(T),
    Err(E)
}
```

#### Union Types (Structural)

```zith
// Anonymous union (structural)
let value: union = switch expr {
    A -> (i32),
    B -> ([]char),
    C -> ()
};

// Named union
let ast: union(Ast) = switch parse() {
    Literal -> (i64),
    Operator -> (char)
};

// Union of types
let num: (i32 | f32) = 42;
```

### 5.4 Generic Types

```zith
struct Box<T> {
    value: unique T
}

fn swap<T>(a: lend T, b: lend T) {
    let tmp = a;
    a = b;
    b = tmp;
}

// Monomorphization: Trait<T> (static dispatch)
// Dynamic Dispatch: Trait*<T> (vtable)
fn process<T: Drawable>(obj: Drawable<T>) { }       // static
fn process_dyn(obj: Drawable*) { }                  // dynamic
```

### 5.5 Traits & Capabilities

Traits define **capabilities** that types can implement.

```zith
trait Copy {
    // Type is bitwise-copyable
}

trait Shared {
    // Safe to use in `share` contexts (thread-safe)
}

trait Lent {
    // Safe to borrow mutably from global `unique`
}

implement Shared for MyType {
    // MyType can be shared across threads
}
```

---

## 6. Type Navigation

### 6.1 Optional Navigation (`?` Operator)

Navigate `?T` types with automatic fallback.

#### Basic Access

```zith
let maybe: ?string = "hello";
let name = ?maybe or "default";      // "hello"

let nothing: ?string = null;
let name = ?nothing or "default";    // "default"
```

#### Chaining with `or` (Short-Circuit)

```zith
let contact = ?user.email or ?user.phone or "no contact";

// Stops at first non-null value
```

#### Type Deduction

If one operand is non-optional (always valid), result is non-optional:

```zith
let v1 = ?opt_a or ?opt_b;           // type: ?T
let v2 = ?opt_a or "default";        // type: string
```

### 6.2 Failable Navigation (`!` Operator)

Navigate `T!` types with error handling.

#### Basic Access

```zith
fn read_file(): []char! {
    if !file_exists(path)
        throw FileNotFound;
    load_file(path)
}

let content = !read_file();
```

#### Error with Context

```zith
let data = !parse_json(input) or JsonError {
    message: "invalid JSON",
    input: input
};
```

#### Chaining with `or` (Short-Circuit)

```zith
let data = !primary_source() or !backup_source() or throw NoData;
```

#### Type Deduction

```zith
fn get_fallback(): i32 { 42 }  // never fails

let v1 = !op! or !op!;              // type: T!
let v2 = !op! or get_fallback();    // type: i32
```

### 6.3 The `must` Operator

Require success or panic.

```zith
let name = must ?user.name;
// If null → panic!("must required valid value at main.zith:42:?user.name")

let data = must !read_file();
// If fails → panic!("must required successful operation at ...")

// In const (becomes static_assert)
const SIZE = must !get_config().size;
// Compile-time failure if cannot determine SIZE
```

### 6.4 The `raw` Operator

Bypass optional/failable checks (still type-safe, unlike `unsafe`).

```zith
let value = raw optional_value;  // access without navigation
```

Use when you **know** the value is valid (external validation, performance-critical).

### 6.5 Private Types (Newtype)

```zith
struct User {
    id: private type UserId(u64),
    age: private type Age(u32)
}

implement Age {
    fn is_adult(self): bool {
        self.value >= 18
    }
}
```

Creates semantically distinct types based on primitives.

---

## 7. Lexical Structure & Operators

### 7.1 Case Style

Zith defaults to **camelCase** (aligning with modern ecosystems).

```zith
let userName = getUser();
fn processData() { }
var currentIndex = 0;
```

### 7.2 Keywords

**Core:**
```
fn, let, var, const, type, struct, enum, union,
if, else, switch, for, while, return, break, continue,
do, error, drop, try, catch, marker, flow, entry, goto
```

**Ownership:**
```
unique, share, view, lend, extension
```

**Advanced:**
```
async, async fn, implement, require, trait, context, use, import,
comptime, static, global, scene, component, entity
```

### 7.3 Operators

```zith
// Arithmetic
+, -, *, /, %, **

// Comparison
==, !=, <, >, <=, >=

// Logical
&&, ||, !

// Bitwise
&, |, ^, <<, >>

// Assignment
=, +=, -=, *=, /=, %=, &=, |=, ^=

// Navigation
?, !, must, raw

// Ownership
as (type casting)

// Pipeline
->

// Access
., [], *

// Pack/Tuple
|...|

// Type Union
|
```

### 7.4 Comments

```zith
// Line comment

/* Block comment
   spanning multiple lines */
```

---

## 8. Contexts (DSL Namespaces)

Contexts are **isolated environments** that define valid syntax, keywords, and operators.

### 8.1 Defining a Context

```zith
context WebApp {
    // Macros
    macro @getParam(name) {
        request.params[name]
    }
    
    // Constants
    const VERSION = "1.0";
    
    // Custom keywords (via macros)
    macro async { /* async handling */ }
    
    // Tag Macros (for HTML-like syntax)
    tag macro div(content) {
        render("<div>", content, "</div>")
    }
}
```

### 8.2 Activating a Context

```zith
context WebApp {
    // Inside this block, WebApp syntax is active
    
    let version = VERSION;
    let param = @getParam("name");
    <div> Hello </div>
}

// Outside, WebApp syntax is not available
```

### 8.3 Context Rules

- **One Global Context:** Only one "global" context active at module level
- **Local Contexts:** Can be opened locally in blocks
- **Namespace Isolation:** Context names don't pollute global namespace
- **Composition:** Can nest contexts

```zith
context Web {
    context Database {
        // Both Web and Database syntax available here
    }
}
```

### 8.4 Tag Macros (DSL Syntax)

```zith
tag macro div(class: string, content) {
    "<div class=\"" + class + "\">" + content + "</div>"
}

context HTML {
    let page = <div class="container">
        <h1>Welcome</h1>
    </div>;
}
```

---

## 9. Packs (Tuples & Ownership)

Packs (`| |`) unify three concepts:

### 9.1 Anonymous Tuples

```zith
let point = | x: 10, y: 20 |;
let values = | 1, 2, 3 |;
```

### 9.2 Variadic Templates

```zith
fn sum<T...>(args: |T...|): T {
    var result = 0;
    for arg in args {
        result += arg;
    }
    result
}

sum(1, 2, 3, 4);   // T... = [i32, i32, i32, i32]
```

### 9.3 Explicit Ownership in Packs

**Default:** Move semantics

```zith
fn process(|a, b|) {
    // a, b are moved (unique by default)
}

// Explicit ownership
fn process(|unique x, share y, view z|) {
    // x: unique (moved)
    // y: share (copied)
    // z: view (borrowed)
}
```

### 9.4 Temporary Borrows (Lend)

`lend` only valid in immediate function invocations:

```zith
fn mutate(data: lend i32) {
    data += 1;
}

// ✅ Lend valid here
mutate(lend x);

// ❌ Cannot store lend in variable
let borrowed: lend i32 = lend x;  // ERROR
```

---

## 10. Control Flow: Markers, Flows & Functions

### 10.1 Functions

#### Standard Function

```zith
fn add(a: i32, b: i32): i32 {
    a + b
}
```

#### Async Function (Coroutine)

Preserves stack state via `yield`.

```zith
async fn fetch_data(url: []char): []char! {
    let response = !network.get(url);
    yield;  // give control to scheduler
    process(response)
}

// Usage in async context
async fn main() {
    let data = await fetch_data("https://...");
}
```

### 10.2 Markers (Structured Goto)

Markers are **labeled blocks within a function** for state machines and event loops.

```zith
fn event_loop() {
    marker wait() {
        let event = event_queue.wait();
        if event.type == QUIT
            goto exit;
        goto process(event);
    }
    
    marker process(event: Event) {
        handle(event);
        goto wait;
    }
    
    marker exit() {
        cleanup();
        return;
    }
    
    entry { goto wait; }
}
```

**Rules:**
- Markers have parameters
- Access parent scope variables (view or lend)
- Cannot move local variables (static analysis)
- Cannot jump to a Scene (unidirectional)

### 10.3 Flows (Reentrant Goto)

For logic that needs to be jumpable from other contexts via anchors.

```zith
flow fn process_global() {
    marker handle(data) {
        // Can be jumped back to via anchor
        process(data);
    }
    
    entry { goto handle(initial_data); }
}
```

**Difference from Markers:**
- Can be re-entered from other code paths
- Anchor-based returns
- Preserves state across jumps

### 10.4 Control Flow Summary

| Type | Use Case | Stack | Reentrant |
|------|----------|-------|-----------|
| `fn` | Normal functions | LIFO | No |
| `async fn` | Coroutines | Preserved | Yes (via yield) |
| `marker` | State machines, loops within function | Preserved | No |
| `flow fn` | Global reentrant logic | Preserved | Yes (via anchor) |

---

## 11. Data-Oriented Architecture: ECS & Scenes

Zith has **first-class support** for data-oriented programming via ECS (Entity Component System).

### 11.1 Components

Plain Old Data (POD) with optional inline functions.

```zith
component Position {
    x: f32,
    y: f32
}

component Velocity {
    dx: f32,
    dy: f32
}

component Health {
    hp: u32,
    max_hp: u32,
    
    fn take_damage(self: lend, damage: u32) {
        self.hp = if self.hp > damage {
            self.hp - damage
        } else {
            0
        };
    }
}
```

### 11.2 Entities (Archetypes)

Container of components with **strong typing**.

```zith
entity Player {
    Position,
    Velocity,
    Health,
    Inventory
}

entity Projectile {
    Position,
    Velocity
}

// Access
let player: Player = /* ... */;
player.Health.take_damage(10);
player.Position.x += 1.0;
```

**Static Entities:** Memory is contiguous, type-safe, zero-overhead.

#### Dynamic Entities

For heterogeneous collections:

```zith
entity mut GameEntity {
    // Dynamic components at runtime
}

let entity: GameEntity = scene.create_entity();
entity.add_component(Position { x: 0, y: 0 });
entity.add_component(Health { hp: 100, max_hp: 100 });
```

### 11.3 Scenes (Memory Regions)

Scenes are **global memory regions** that organize entities and resources.

```zith
scene GameWorld(require: 100 entities) {
    // Allocate space for ~100 entities
    
    // Policy: what happens if memory fills?
    policy: Terminate              // kill oldest (default)
    policy: Grow                   // dynamically grow
    policy: Circular               // FIFO buffer
    policy: Define { /* custom */ }
}
```

**Lifetime:** Scene memory is allocated once, entities live within it.

#### Scene as Program Organizer

```zith
// Each scene organizes a logical region
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

### 11.4 Entity to Struct Conversion (Threading)

To send entities across threads, convert to plain struct:

```zith
// Extract components into struct
let struct_data = @toStruct(player);
// struct_data: { Position, Health, Inventory }

// Send to thread
thread::spawn(|| {
    // Use struct data
    process(struct_data);
    
    // Reconstruct entity (if needed)
    let reconstructed = @toEntity(struct_data, scene);
});
```

The `@toStruct` intrinsic creates a serializable struct from entity components.
The `@toEntity` intrinsic reconstructs it back.

### 11.5 Scene Rules

- **Always Global:** Scenes cannot be local/scoped
- **Memory Contiguity:** Compiler allocates contiguously when possible
- **Component Safety:** Components are type-checked at compile-time
- **Dynamic Components:** mut entities allow runtime component addition

---

## 12. Intrinsics (@)

Intrinsics are **compile-time metadata/reflection** accessed via `@` operator.

### 12.1 Type Information

```zith
// Size of type
let size = @sizeOf(T);

// Members of struct
for member in @members(T) {
    println!("{}: {}", member.name, member.type);
}

// Is type copyable?
if @hasTrait(T, Copy) {
    // T can be bitwise-copied
}
```

### 12.2 Location Information

```zith
// Current file
let file = @file();

// Current line
let line = @line();

// Current function
let fn_name = @funcName();

// Create rich panic messages
panic!(@location() + ": something went wrong");
```

### 12.3 Entity/Component Intrinsics

```zith
// Components of an entity type
for comp in @components(Player) {
    println!("{}", comp);  // Position, Health, ...
}

// Convert entity to struct
let struct_data = @toStruct(player);

// Convert struct back to entity
let entity = @toEntity(struct_data, scene);

// Register component dynamically
@registerComponent(mut entity, NewComponent { /* ... */ });
```

### 12.4 Compile-Time Evaluation

```zith
// Constexpr-like
const ARRAY_SIZE = @sizeOf([10]i32) / @sizeOf(i32);  // 10

// Must be compile-time computable
const MEM_LAYOUT = @members(MyStruct);
```

---

## 13. Error Handling & Try/Catch

### 13.1 Failable Types (`T!`)

Functions return `T!` if they can fail:

```zith
fn divide(a: i32, b: i32): i32! {
    if b == 0
        throw DivisionByZero;
    a / b
}
```

### 13.2 Throwing Errors

```zith
throw MyError("description");
throw MyError { field: value };
```

### 13.3 Try/Catch

```zith
try divide(10, 2) | result | {
    println!("success: {}", result);
} catch {
    DivisionByZero -> println!("divide by zero"),
    _ -> println!("other error")
}
```

### 13.4 Do/Error/Drop

Structured resource management:

```zith
do ProcessFile {
    let file = !File.open("data.txt");
    let data = !file.read();
    process(data)!;
}
error ProcessFile {
    // Add context and re-throw
    throw FileProcessError {
        context: "failed to process file",
        cause: error
    };
}
drop ProcessFile {
    // Always executes (cleanup)
    file.close();
}
```

**Execution Order:**
1. `do` — acquire resources and work
2. `error` — intercept errors (if error occurred)
3. `drop` — cleanup (always)

### 13.5 Error Propagation

```zith
fn process_file(path: []char): void! {
    let file = !File.open(path);
    let data = !file.read();
    let parsed = !parse_json(data);
    
    apply(parsed)
    // Errors propagate automatically
}
```

---

## 14. Concurrency & Globals

### 14.1 Global Variables

Static variables with ownership rules.

```zith
// Must implement Shared trait (thread-safe)
global score: share i32 = 0;

// Must implement Lent trait (atomic access)
global counter: unique i32 = 0;

// Access
global score += 10;
```

### 14.2 Thread Safety

- **`global: share`** — requires `Shared` trait (explicitly thread-safe)
- **`global: unique`** — requires `Lent` trait (atomic operations)

### 14.3 Structured Concurrency

```zith
fn concurrent_work() {
    let handle = thread::spawn(|| {
        // Conversion to struct for threading
        let data = @toStruct(player);
        process(data);
        data
    });
    
    let result = handle.join();  // Must join before scope ends
    // OR use Arc/Rc for unbounded sharing
}
```

### 14.4 Arc & Rc (Reference Counting)

Available in standard library for unbounded shared ownership:

```zith
let arc_data = Arc.new(data);
let clone1 = arc_data.clone();
let clone2 = arc_data.clone();

// All owners share, reference counted
```

---

## 15. Standard Library

### 15.1 Three-Part Structure

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

### 15.2 Common Modules

```zith
std/io/console{println, print, eprint}
std/collections{Vec, Map, Set}
std::thread::spawn, join
std::memory::allocate, deallocate
std::fs::File, open, read, write
std::json::parse, serialize
```

### 15.3 Traits

```zith
trait Copy { }
trait Shared { }
trait Lent { }
trait ToString { }
trait Drawable { }
```

---

## 16. Zith vs Other Languages

### 16.1 Memory Model

| Language | Model | Complexity | Safety |
|----------|-------|-----------|---------|
| **Rust** | Affine (Lifetimes) | High | ✅ |
| **C** | Manual Pointers | Very High | ❌ |
| **Go** | GC | Low | ✅ |
| **Zig** | Manual Allocators | Medium | ⚠️ |
| **Zith** | NRM (Connectivity) | Medium | ✅ |

**Zith Advantage:** Safety without lifetime complexity. Weak references replaced by `extension`.

### 16.2 Data-Oriented Programming

| Language | ECS | Native | Ergonomic |
|----------|-----|--------|-----------|
| **Rust** | Via Crates | ❌ | Hard |
| **C#** | Via Libraries | ❌ | Medium |
| **Zig** | Manual | ❌ | Hard |
| **Zith** | ✅ Native | ✅ | ✅ Easy |

**Zith Advantage:** First-class support, no libraries needed.

### 16.3 DSLs & Metaprogramming

| Language | DSL Support | Syntax | Ease |
|----------|-----|--------|--------|
| **Lisp** | ✅ Excellent | Prefix | Hard |
| **Rust** | Via Macros | Procedural | Medium |
| **Zith** | ✅ Contexts + Tags | Native | Easy |

**Zith Advantage:** Tag macros for HTML-like syntax, clean DSL definition.

---

## 17. Best Practices & Patterns

### 17.1 Ownership Patterns

**Default to `unique`:**

```zith
let resource = unique Resource.new();
```

**Use `share` for intentional multiple owners:**

```zith
let data = shared_database.clone();  // implement Share
let handle1 = data as share;
let handle2 = data as share;
```

**Use `view` for reading:**

```zith
fn process(config: view Config) {
    // read-only access
}
```

**Use `lend` for temporary mutation:**

```zith
fn update(state: lend GameState) {
    state.update();
}
```

**Use `extension` for part-of relationships:**

```zith
struct TreeNode<T> {
    value: T,
    children: []share TreeNode<T>,
    parent: extension TreeNode<T>?
}
```

### 17.2 Optional & Failable Patterns

**Prefer `?...or` for optionals:**

```zith
let name = ?user.name or "guest";
```

**Prefer `!...or` for fallaibles:**

```zith
let config = !load_primary() or !load_backup() or default_config();
```

**Avoid `must` except for initialization:**

```zith
// ✅ OK: initialization failure is fatal
const API_KEY = must !env("API_KEY");

// ❌ Avoid: mid-function panic
let value = must ?optional;
```

### 17.3 Context Patterns

**Group related DSL features:**

```zith
context Database {
    macro @query(sql) { execute(sql); }
    tag macro @table(name) { create_table(name); }
    const DEFAULT_TIMEOUT = 5000;
}
```

**One global context per domain:**

```zith
// main.zith
context WebServer {
    // All web-related DSL here
}

// game.zith
context GameEngine {
    // All game-related DSL here
}
```

### 17.4 ECS Patterns

**Static entities for performance:**

```zith
entity Player {
    Position, Health, Inventory  // fixed components
}

// Contiguous memory, zero indirection
```

**Dynamic entities for flexibility:**

```zith
entity mut GameObject {
    // runtime flexibility
}
```

**Scene per logical region:**

```zith
scene MainLevel {
    // All main level entities/resources
}

scene BossRoom {
    // Boss-specific entities
}
```

### 17.5 Error Handling Patterns

**Add context to errors:**

```zith
do LoadConfig {
    let file = !File.open(path);
    let data = !file.read();
}
error LoadConfig {
    throw ConfigError {
        message: error.message,
        file: path,
        suggestion: "check file permissions"
    };
}
```

**Use error chains:**

```zith
let data = !step1() or !step2() or !step3() or throw AllFailed;
```

---

## Appendix A: Complete Examples

### A.1 Event-Driven Game Loop

```zith
scene GameWorld(require: 1000 entities) {
    policy: Grow;
}

fn game_loop() {
    marker wait_event() {
        let event = window.poll_event();
        if event.type == QUIT {
            goto shutdown;
        }
        goto process_event(event);
    }
    
    marker process_event(event: Event) {
        match event.type {
            MOUSE_MOVE -> {
                // update player position
            },
            KEY_PRESS -> {
                // handle input
            },
            _ -> {}
        }
        goto render;
    }
    
    marker render() {
        for entity in scene.GameWorld.entities {
            if entity.has_component(Position) && entity.has_component(Sprite) {
                draw(entity);
            }
        }
        goto wait_event;
    }
    
    marker shutdown() {
        cleanup();
        return;
    }
    
    entry { goto wait_event; }
}
```

### A.2 Web Server with Context

```zith
context WebServer {
    macro @getParam(name) {
        request.params[name]
    }
    
    tag macro <div class="">(content) {
        "<div class=\"...\">" + content + "</div>"
    }
}

context WebServer {
    fn handle_request(request: view HttpRequest): HttpResponse! {
        match request.path {
            "/api/user/:id" -> {
                let id = @getParam("id")!;
                let user = !database.get_user(id);
                
                return HttpResponse {
                    status: 200,
                    body: <div class="user">
                        @serialize(user)
                    </div>
                };
            },
            _ -> {
                throw HttpError { status: 404 };
            }
        }
    }
}
```

### A.3 JSON Parser with ECS

```zith
component JsonValue {
    type: u32,  // literal, object, array
    data: *void
}

entity mut JsonDocument {
    // Dynamic components for different value types
}

scene JsonParser {
    policy: Grow;
}

fn parse_json(input: []char): JsonDocument! {
    do Parse {
        var pos = 0;
        let doc = scene.JsonParser.create_entity();
        let value = !parse_value(input, lend pos, doc);
        return doc;
    }
    error Parse {
        throw JsonError {
            message: error.message,
            position: pos
        };
    }
}
```

---

## Appendix B: Syntax Quick Reference

### Variables
```zith
let x = value;              // immutable
var x = value;              // mutable
const X = value;            // compile-time
global x: share Type = ...  // static
```

### Types
```zith
i32, u64, f32, bool, char, void
?T, T!, (T, U)
[N]T, []T
struct { }, enum { }, union { }
unique T, share T, view T, lend T, extension T
```

### Functions
```zith
fn name(params): Return { }
async fn name() { yield; }
flow fn name() { }
marker label(params) { goto ... }
```

### Control Flow
```zith
if cond { } else (cond) { }
for i in range { }
switch expr { pattern -> value, _ -> default }
try op() | result | { } catch { }
do Block { } error { } drop { }
```

### Contexts & Macros
```zith
context Name { macro fn @name() { } }
context Name { tag macro <div>(content) { } }
```

### ECS
```zith
component Type { fields; }
entity Name { Component, Component }
scene Name(require: N entities) { policy: ...; }
@sizeOf(T), @members(T), @toStruct(entity), @toEntity(struct)
```

---

## Appendix C: Zith Vocabulary

| Term | Meaning |
|------|---------|
| **Node** | Any value/data in Zith |
| **Edge** | Variable (reference to a node) |
| **NRM** | Node Resource Model (foundation) |
| **Keyword** | Rule applied to nodes (unique, share, view, lend, extension) |
| **Context** | DSL namespace with custom syntax |
| **Pack** | Tuple with optional ownership modifiers |
| **Marker** | Labeled block for state machines |
| **Scene** | Global memory region organizing entities |
| **Entity** | Container of components |
| **Component** | Plain data struct |
| **Intrinsic** | Compile-time metadata (accessed via @) |
| **Zombie** | Node invalidated in current branch |

---

## Summary

Zith provides:

✅ **Memory Safety** — NRM + 5 Keywords eliminate undefined behavior  
✅ **Universal Applicability** — One language for kernels, games, web, UI  
✅ **Data-Oriented** — Native ECS removes boilerplate  
✅ **DSL Friendly** — Contexts + Tag Macros for any domain  
✅ **Low Learning Curve** — Start simple, grow powerful as needed  
✅ **Zero-Cost** — High-level abstractions compile to efficient code  

---

**Status:** Version 2.0 Complete  
**Repository:** https://github.com/galaxyhaze/Zith  
**Documentation:** https://galaxyhaze.github.io/Zith  
**Last Updated:** April 2026
