# Stdlib I/O Result Contract And Generic Union Limits

This note records the proposed `Result<T, E>` surface for stdlib I/O and the
compiler facts that currently block convenient generic `Ok`/`Err` helpers.
It is deliberately separate from `memory/stdlib-io-format.md`, which records
the `format`/`FormatBuffer` design.

## Current Compiler Facts

Generic unions work for concrete instantiations:

```zith
union Result<T, E> {
    T,
    E,
}
```

The supported extraction path is tagged/raw union casting, not named variant
access:

```zith
let n: u32 = raw result as u32;
```

Free generic helpers that construct and return a generic union do not compile
today. The repro below reports `E3001 raw union member type mismatch` and
`E3011 cannot infer generic argument`; it is the exact blocker that led the
design to direct union construction instead of `Ok<T>(value)`/`Err<E>(value)`.
The C++ test checks for `err::TypeMismatch` (`E3001`) or
`err::GenericCannotInfer` (`E3011`), matching the currently observed
diagnostics:

```zith
union Result<T, E> {
    T,
    E,
}

fn Ok<T, E>(value: T): Result<T, E> {
    var r: Result<T, E> = Result<T, E>{ value };
    return r;
}

fn main(): i32 {
    let a: Result<u32, u32> = Ok<u32, u32>(7);
    return 0;
}
```

`Result<void, E>` is intentionally out of scope for the first stdlib I/O
version. There is no validated `()` generic argument and `null` to `void`
would be speculative. The sink overload returns `IoError` instead.

## Adopted Surface

The first stdlib I/O version does not use a generic union for `format`.
`FormatResult` is a plain struct that keeps the owned `FormatBuffer` and an
`IoError` status together:

```zith
pub struct FormatResult {
    buffer: FormatBuffer,
    status: IoError,
}
```

`format(msg, values)` returns `FormatResult`. Callers use `result.error()`,
`result.text()`, and `result.destroy()`. Tuple returns and variadic forwarding
are not supported, which are the concrete reasons the original
`Result<FormatBuffer, IoError>` tuple-like plan was replaced.

## Design Decision

Generic unions remain valid for concrete instantiations and direct union
construction plus `raw as` extraction:

```zith
Result<FormatBuffer, IoError>{ buffer }
Result<FormatBuffer, IoError>{ IoError.fWrite }
```

Those helpers can be introduced when generic union helper inference works.
Convenient `Ok`/`Err` helpers remain a future compiler work item, not part of
this stdlib change.
