## 8. Error Handling

> **Implementation status:** `?T` is **working in Zith--** with `?` postfix propagation, full
> operand and return-type validation, `null → ?T` and `T → ?T` coercions, and optional extraction
> via `must`/`raw`. `T!` and the `!` propagation family are full-Zith only. `fail`, `with`,
> `catch`, `throw`, `try`, and `try ... or` fallback are **spec-only**.
> See [impl-status.md](impl-status.md).


Error handling in Zith is fully static and return-based. There are no exceptions, and no semicolon is required after `?` or `!`.

### 8.1 Failable Types

| Syntax | Meaning | Propagated by |
|---|---|---|
| `?T` | Optional — `T` or `null`. Also the Zith-- optional type. | `?` (postfix) |
| `T!` | Result — `T` or an error. Full Zith only; equivalent to a `Result<T, E>` where `E` implements `Error`. The compiler infers an anonymous error union when multiple error types are possible. | `!` (postfix) |

In full Zith, failable types may be stacked, and the notation reads linearly:

```zith
?*?(?i32 ! IoError)
```

Read left to right, outer to inner: an *optional* **pointer** to an *optional* **Result**, where the Result's success type is `?i32` and its error type is `IoError`.

> **Zith-- boundary:** `?T` is part of Zith--. `T!` is a full-Zith type, and the Zith-- pipeline
> has no failable-result syntax; error propagation remains in the full spec.

### 8.1.1 C pointers are `?*T`

Every pointer imported from a C header is typed `?*T`, not `*T`: a C pointer is nullable, and
`?*T` uses the nullptr niche, so the layout is exactly the bare pointer. `is null` is the
canonical way to check one:

```zith
import "stdio.h"

let f = fopen("data.bin", "r");   // f: ?*FILE
if (f is null) {
    return 1;
}
```

Comparing a pointer against an integer is an error, including the C idiom `p != 0` and its hex
spelling `p != 0x0`: no integer-to-pointer coercion exists.

Reinterpreting a C pointer keeps its nullability: the target of the cast must itself be
nullable, so `as ?*T` is the accepted form and `as *T` reports `E3003` with a diagnostic that
names the `?*T` spelling.

```zith
import "stdlib.h"

let cell = malloc(64) as ?*i32;   // ok: cell is ?*i32
let bad  = malloc(64) as *i32;    // E3003: use 'as ?*T'
```

In the other direction no cast is needed at all: any pointer, nullable or not, is accepted
where a C `void*` (`raw opaque`) is expected, so `free(cell)` and `free(&local)` both compile.
That coercion is one-way. Going from `raw opaque` back to a concrete `?*T` still requires `as`.

`?*T` narrows to `*T` only inside a branch proven by `is null`/`not (is null)`
(including the stdlib style `if (p is null) { return ...; }`, whose early return
keeps the proof for code after the `if`). An unchecked use in deref, arrow, index,
or coercion expects `*T` and reports `E3005 NullDerefUnproven`; `raw` remains the
explicit opt-out for unchecked pointer reads.

### 8.2 `must` vs. `raw`

| | Debug mode | Release mode |
|---|---|---|
| `must` | Panics with file and line information. | The compiler guides you to remove it, turning it into an `if`/`else` with an early return and a custom error code. |
| `raw` | Always unchecked. | Always unchecked. |

```zith
let cfg: ?Config = tryLoad();
let c1 = must cfg;   // panics in debug; compiler warns/guides in release
let c2 = raw cfg;    // always unchecked; compiler always warns
```

`must` also doubles as an assertion: `must(cond)` panics with file and line info if `cond` is false (debug only). In release, the compiler guides you to replace it with proper error handling.

### 8.3 `try`, Propagation, and Fallback

```zith
fn readConfig(path: string): Config! {
    let file = File.open(path)!
    let data = file.read()!
    parse(data)!
}

let name = try user.name or "guest";
let data = try primary() or backup() or default;

// Propagation inside a chain
readFile("data.bin") -> parse(..)! -> validate(..)? -> process(..)
```

Accessing a failable type's inner value requires one of `try`, `!`, `raw`, or `must`.

- **`try`** short-circuits only the enclosed expression and keeps the failure as a local value.
- **`!`** unwraps and propagates a failure out of the enclosing scope.
- **`raw`** unwraps without checking the failure.
- **`must`** unwraps and asserts that the value is valid.

#### `try` and `or`

`try` guards a single expression. It stops the expression at the first failure and does not
route that failure to a `fail` scope guard. It is the local fallback form; the fix is `try expr`
with no prefix `?`/`!`.

```zith
// Local fallback; the failure is consumed in this expression
let x = try opt or default;
let x = try opt or default or backup; // valid — chain of fallbacks

// Short-circuit with a failure value: a, foo(), or c() may stop the chain
let value = try a.foo().c();
```

`try expr` has a union type (`T | failures`); the result is either an integral `T` or the
failure that stopped the expression. `try expr or fallback` collapses that result to `T`.
`or` is short-circuiting and evaluates only until an integral result is found.

#### Postfix `!`

Postfix `!` propagates a failure out of the current scope. Unlike `try`, this is the path
that can activate a surrounding `fail` block. Postfix `?` keeps the existing optional
propagation role for `?T`.

```zith
// Postfix — propagates from the failing segment to the enclosing scope
let x = y.data()!fn()!process()!
```

Prefix `?`/`!` fallback forms are removed from the language surface; `try ... or` is their
replacement.

### 8.4 `with` / `catch`

| Form | Behavior |
|---|---|
| `with` | Short-circuit — the first failure jumps straight to `catch` |
| `eager with` | Eager — every expression is evaluated; `catch` runs if any failed |

```zith
// Short-circuit
with (connectDb(), user: getUser(id)) {
    process(user);
}
catch (err) { log(err); }   // any name works; 'err' is convention

// Eager — all expressions run, then catch if any failed
eager with (a: fetchA(), b: fetchB()) {
    use(b);
} catch { log(a, b); }
```

> In `eager with`, all expressions are evaluated before `catch`. The named bindings (`a`, `b`) remain in scope inside `catch` so you can inspect which ones failed. In short-circuit `with`, only the failing expression is known, so `catch` receives a single error parameter.

### 8.5 `fail` Blocks

A `fail` block runs when an error would otherwise escape its associated scope. It is a scope
listener, not an expression-level fallback. Only `!` propagation and `throw` activate it;
`try` keeps failures local and does not reach `fail`. A `fail` block can follow a named block
(external) or sit inside a block as a scope guard (nameless):

```zith
// External fail
loadConfigure {
    let raw = readFile("config.json")!
    parse(raw)!
} fail loadConfigure(err) {
    if (err is NotFound) { continue(default); }
    throw Error{ context: "load failed", cause: err };
}

// Nameless fail -- guards the current scope
{
    fail (err) { log("scope error:", err); }
    risky()!
    another()!
}
```

> **Name linking:** an external `fail` block's name must match the block it guards. When there is only one failable block in scope, the name can be omitted. A nameless `fail` guards the current scope directly. The compiler passes the error the same way.

Inside a `fail` block, the parameter receives the error directly. This is the difference
from `try ... or`: `try` discards or collapses the failure, while `fail` has the original
error available for logging, transformation, or conditional recovery. You have four options:

- `continue(value)`, to resume after the block with a replacement value.
- `return value;`, to exit the enclosing function.
- `throw value;`, to propagate a new error (requires the `Error` capability).
- Fall through, so the original error propagates unchanged.

Use `@ok` to extract the success type from the failure node. This helps when `continue` needs to return a value of a different type than the error:

```zith
fail (err) {
    continue(@ok err);   // extract success value from the failure node
}
```

`@err` also exists for extracting the error type in other contexts.

### 8.6 `throw`

```zith
fn divide(a: i32, b: i32): i32! {
    if (b == 0) throw DivisionByZero;
    a / b
}
```

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
