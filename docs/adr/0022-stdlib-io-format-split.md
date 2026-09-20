# Stdlib I/O Format Split

## Status

Accepted as scoped implementation; module split deferred.

## Context

`std/io/console` currently mixes `input()`/`InputLine`/`ParseInput` with
`print`/`println`/formatting. The stdlib needs clearer seams: reading from a
console should not depend on the format contract, writing should make
formatting reusable by user types, and current examples/tests must not break.
The compiler currently blocks the full split in two ways: qualified facade
re-export cannot preserve `std.io.console.*`, and variadic forwarding between
stdlib functions is rejected.

The existing `Formatable` trait and `#`-placeholder runtime format are the
stable parts of the current console path. `FormatBuffer` owns heap storage and
is reused by `print`/`println`. The proposed change adds a sink abstraction so
formatting can write into `FormatBuffer` or a caller-provided fixed slice.
The intended abstraction is `TextSink`, but it is not usable yet because the
compiler blocks mutable `dyn`/`lend` receivers.

## Decision

Keep the split scoped and deferred to the compiler-facade debt. In this
iteration, `format.zith` owns `Formatable`, `FormatBuffer`, `FormatResult`,
`IoError`, and the owned free `format`. `console.zith` remains the real module
for `InputLine`, `ParseInput`, `input()`, and the existing `print`/`println`.
The future split into `consoleIn`/`consoleOut` plus a qualified facade is
recorded in `docs/implementation-debt.md` and `memory/stdlib-io-format.md`.

The adopted owned result replaces a speculative `Result<T, E>` plan because
tuple returns are unsupported and generic `Ok`/`Err` helper inference is
blocked:

```zith
pub trait Formatable {
    fn format(self, dest: lend FormatBuffer): IoError;
}

// Intended once mutable dyn receivers work:
// pub trait TextSink {
//     fn append(self: lend dyn TextSink, chars: []char): IoError;
// }
// pub trait Formatable {
//     fn format(self, dest: lend dyn TextSink): IoError;
// }

pub struct FormatResult {
    buffer: FormatBuffer,
    status: IoError,
}

pub fn format(msg: []char, values: [...]dyn Formatable): FormatResult;
```

The owning version owns storage through the default heap for now; a future
`format_with(allocator, ...)` will accept an explicit allocator.

## Consequences

Current console examples and tests keep compiling with their old HIR names.
`format` is available through `from std/io/format`; qualified facade access via
`std.io.console.*` is a documented compiler debt. The new API is blocked on the
recorded compiler facts: generic `Ok`/`Err` union helpers, qualified trait
receivers, mutable `dyn`/`lend` sink receivers, tuple returns, variadic
forwarding, and facade namespace segments.
