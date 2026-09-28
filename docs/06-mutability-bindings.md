## 6. Mutability & Bindings

> **Implementation status:** `let`, `var`, and `const` bindings are **working**. Pack literals,
> positional indexing, and binding destructuring with `[ ]` are working in Zith--. The full
> mutability model remains partly spec-only.
> `lend` and `view` are implemented as the Zith-- call-annotation slice; `own`, `share`,
> `belong`, and the full NRA state machine remain full-Zith/spec-only.
> See [impl-status.md](impl-status.md).

### 6.1 Deep Mutability Model

Zith uses deep mutability: a modifier on a binding flows into every nested field. Fields inside a struct inherit the mutability of the instance that holds them. No per-field `mut` annotation is needed.

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

// lend, own, share, belong → imply mut
fn update(p: lend Point) { p.x += 1; }  // p is mutable (lend implies mut)
let r: own Resource = acquire();     // r's fields are mutable (own implies mut)

// view → implies immutable
fn read(c: view Config) { ... }         // c is read-only (view implies immutable)

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

Packs group heterogeneous values into a tuple-like aggregate. Zith calls this
construct a **pack**, not a tuple. A pack literal uses `| |` and keeps its
member order and member types:

```zith
let p = | 5, 4, 'c' |;       // pack<i32, i32, char>

let [a, b, c] = p;

let first: i32 = p[0];
let letter: char = p[2];
```

Packs are heterogeneous. Each position can have a different type, and the
compiler knows the number, order, and concrete layout of the members at
compile time. Indexes must be compile-time integer positions, because a pack
does not represent a homogeneous runtime array.

Use destructuring when the positions need names in the current scope:

```zith
fn split(): |i32, char| {
    |42, 'x'|
}

fn main(): i32 {
    let [number, symbol] = split();
    number
}
```

Use a pack as a loop accumulator when a loop needs to carry more than one
value. The accumulator annotation describes the pack members:

```zith
let result = for ([acc, i]: i32), (i in 0..n) {
    acc *= i + 1
} or 0;
```

Packs are also valid as values in aggregate contexts, including `enum:union`
variants. They are not anonymous functions, and they do not provide named
fields unless a named pack type supplies those fields.

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
