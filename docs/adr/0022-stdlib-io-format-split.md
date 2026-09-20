# Stdlib I/O Format Split

## Status

Proposed.

## Context

`std/io/console` currently mixes `input()`/`InputLine`/`ParseInput` with
`print`/`println`/formatting. The stdlib needs clearer seams: reading from a
console should not depend on the format contract, writing should make
formatting reusable by user types, and current examples/tests must not break.

The existing `Formatable` trait and `#`-placeholder runtime format are the
stable parts of the current console path. `FormatBuffer` owns heap storage and
is reused by `print`/`println`. The proposed change adds a sink abstraction so
formatting can write into `FormatBuffer` or a caller-provided fixed slice.

## Decision

Split `console` into `consoleIn` and `consoleOut`, keeping `console` as a
compatibility facade that re-exports both sides.

`format.zith` owns `Formatable`, `TextSink`, `Result<T, E>`, primitive format
implementations, and two free `format` overloads:

```zith
pub trait Formatable {
    fn format(self, dest: lend dyn TextSink): IoError;
}

pub trait TextSink {
    fn capacity(self): u64;
    fn length(self): u64;
    fn append(self: lend dyn TextSink, chars: []char): IoError;
    fn text(self): []char;
}

pub fn format(sink: dyn TextSink, msg: []char, values: [...]dyn Formatable): Result<(), IoError>;
pub fn format(msg: []char, values: [...]dyn Formatable): Result<FormatBuffer, IoError>;
```

The sink overload uses `IoError` because `Result<void, E>` is not yet
validated. The owning overload uses `Result<FormatBuffer, IoError>`; the first
version owns storage through the default heap, and a future
`format_with(allocator, ...)` will accept an explicit allocator.

`consoleIn` owns `InputLine`, `ParseInput`, and `input()` without depending on
`format`. `consoleOut` exposes `print`/`println` and depends on `format`.
both names are re-exported by the `console` facade so existing
`std/io/console.print`/`std/io/console.println` consumers continue to work.

The receiver qualifiers on `TextSink` are provisional: a probe showed that
`self: view Self`/`self: lend Self` in trait requirements does not currently
match an implementation, so simple receivers are used first and qualified
receivers will be revisited after compiler support is proven.

## Consequences

Current console examples and tests keep compiling through the facade.
`Formatable` becomes independent of `FormatBuffer`, letting user types format
into any supported sink. The new API is blocked on the recorded compiler
facts: generic `Ok`/`Err` union helpers and qualified trait receivers need
future compiler work.

