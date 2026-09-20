# Stdlib I/O Format Design

This note records the proposed `std/io` format split and the probe facts that
must be checked before implementing the new stdlib files. The current public
entry point is `stdlib/std/io/console.zith`; it must remain a compatibility
facade while `consoleIn` and `consoleOut` become the real modules.

## Target Layout

- `stdlib/std/io/format.zith`: `Formatable`, `TextSink`, `Result<T, E>`, the
  two free `format` overloads, and primitive `Formatable` implementations.
- `stdlib/std/io/consoleIn.zith`: `InputLine`, `ParseInput`, and `input()`.
- `stdlib/std/io/consoleOut.zith`: `print`/`println` built on `format`.
- `stdlib/std/io/console.zith`: facade re-exporting both sides for current
  examples and tests.

## Public Format Surface

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

The sink overload intentionally avoids `Result<void, E>` because `()`/`void`
as a generic argument is not validated. The owning overload creates a
`FormatBuffer` with the default heap for now; `format_with(allocator, ...)`
is the planned future explicit-allocator path.

The placeholder grammar stays the same as `print`/`println`: `#` marks one
positional value and `\#` is the literal escape. `FormatBuffer` can grow; a
fixed `[]char` sink cannot grow and reports an I/O error when full.

## Probe Facts Recorded

`dyn TextSink` with simple trait receivers compiles. A first probe using
`self: view Self`/`self: lend Self` in trait requirements failed with
`E2022`; the design therefore starts with simple receivers and validates
qualifier support separately before adding `view`/`lend` requirements.

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

The target `format` should keep this bridge shape and call
`formatNext(raw values[next], lend sink)`.

For the stdlib, `dyn` is the target because most stdlib surfaces want one
shared vtable path and less code bloat. `TextSink` generic monomorphization is
still useful as a probe: `fn write<T: TextSink>(sink: T)` should be tested for
correctness before relying on dynamic dispatch everywhere.

