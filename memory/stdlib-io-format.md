# Stdlib I/O Format Design

This note records the current `std/io` format implementation and the compiler
facts that constrain the facade plan. `stdlib/std/io/format.zith` is the real
formatting module; `stdlib/std/io/console.zith` remains the real module for
`print`/`println`/`input` and exposes the old HIR surface used by tests.

## Target Layout

- `stdlib/std/io/format.zith`: `Formatable`, `FormatBuffer`, `FormatResult`,
  `IoError`, the owned `format` free function, and primitive `Formatable`
  implementations.
- `stdlib/std/io/console.zith`: `InputLine`, `ParseInput`, `input()`, and the
  existing `print`/`println` wrappers. It imports `std/io/format` with `from`.

## Public Format Surface

```zith
pub trait Formatable {
    fn format(self, dest: lend FormatBuffer): IoError;
}

pub struct FormatResult {
    buffer: FormatBuffer,
    status: IoError,
}

pub fn format(msg: []char, values: [...]dyn Formatable): FormatResult;
```

`format` creates a `FormatBuffer` with the default heap for now;
`format_with(allocator, ...)` is the planned future explicit-allocator path.
The result is owned and must be destroyed with `result.destroy()`. `text()`
returns the view slice and `error()` returns the `IoError` status.

The placeholder grammar stays the same as `print`/`println`: `#` marks one
positional value and `\#` is the literal escape. `FormatBuffer` can grow; there
is no fixed-slice sink yet.

## Intended Target Surface

The domain contract for a sink is `TextSink`. Once mutable dynamic receivers
work, `Formatable` should accept that contract and primitives should append
through a reusable sink instead of only through `FormatBuffer`:

```zith
pub trait TextSink {
    fn capacity(self): u64;
    fn length(self): u64;
    fn append(self: lend dyn TextSink, chars: []char): IoError;
    fn text(self): []char;
}

pub trait Formatable {
    fn format(self, dest: lend dyn TextSink): IoError;
}
```

This is the target recorded in `docs/adr/0022-stdlib-io-format-split.md` and
the compiler debt for mutable dyn receivers.

## Probe Facts Recorded

`TextSink` is the intended target contract for sinks. The first stdlib surface
uses `FormatBuffer` because mutable `dyn`/`lend` receivers are still blocked:
`lend dyn TextSink` fails with `E3001`/`E2007`, and a mutable trait receiver
such as `append(var self, ...)` on `dyn TextSink` erases to a copy instead of
the original storage. `FormatBuffer` is therefore the concrete sink until the
compiler fixes qualified trait receivers and dyn erasure for mutable
receivers.

Calling a method directly on a variadic `dyn Formatable` element does not
compile today:

```zith
let s = raw values[i].format();
```

The current stdlib bridge still works:

```zith
fn formatNext(value: dyn Formatable, buffer: lend FormatBuffer): IoError {
    return value.format(lend buffer);
}
```

`format` keeps this bridge shape and calls
`formatNext(raw values[next], lend buffer)`.

Tuple returns are not supported, so the owned result uses a plain struct
instead of a tuple or generic `Result<T, E>` helper. Variadic slices cannot be
forwarded into another variadic function, so `print`/`println` keep their own
loop instead of delegating to `format`.

`export std/io/format` cannot preserve `std.io.console.println` qualified
access while also re-exporting the format symbols without duplicate `std`
bindings. The documented facade split remains a compiler debt; for now
`from std/io/format` is the supported bare import path.
