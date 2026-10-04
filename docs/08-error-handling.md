## 8. Error Handling

> **Implementation status:** The current Zith-- compiler still supports legacy `?T` optional
> types and `?` propagation. This support is not the full-Zith failable-state design described
> below. `T!` return annotations, `!` propagation, `Failable`, `Invalid`, `fail`, `with`,
> `catch`, `throw`, `try`, and `try ... or` fallback are spec-only.
> See [impl-status.md](impl-status.md).


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

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
