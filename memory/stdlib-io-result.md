# Stdlib I/O Result Contract And Generic Union Limits

This note records the `Result<T, E>` surface for stdlib I/O and the compiler
facts behind convenient generic `Ok`/`Err` helpers. It is deliberately separate
from `memory/stdlib-io-format.md`, which records the `format`/`FormatBuffer`
design.

The canonical surface now ships as `stdlib/std/result.zith`: the positional
`pub union Result<T, E> { T, E }` plus `Ok`/`Err`/`isOk`/`isErr`/`unwrap`/
`unwrapErr` generic helpers. Result-only parameters infer from the expected
result type (a binding annotation or a declared return type), so `Ok(7)` and
`new(3)` bind `T`/`E` without an explicit `<...>` list. `test-stdlib-modules`
type-checks the module and `test-codegen` (`test_imported_result_runtime`) runs
an imported `Result<T, E>` end-to-end.

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

Free generic helpers that construct and return a generic union now compile.
The earlier blocker was in generic-union monomorphization: `substituteType`
built the concrete instance name from the template's unsubstituted members
(`T`/`E`), so every instance collapsed to `Result<T,T>`. It now names the
instance from the substituted members, so the helper below checks and runs.
`Ok`/`Err` remain plain generic helpers; they are not named union cases
(named cases are not part of the language). The C++ regression lives in
`test-enum-union-generics` (`test_generic_union_instances_stay_distinct`):

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

Generic union helper inference now works (see the `Ok`/`Err` example above), so
`Result<T, E>` helpers are shipped in `stdlib/std/result.zith`. The first I/O
version still keeps the plain `FormatResult` struct, so the I/O surface is
unchanged; `std/result` is available to new callers.
