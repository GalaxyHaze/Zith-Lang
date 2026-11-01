# Variadic Macro Repetition And Caller-Scope Defer

Status: drawing

Zith-- macros currently require exact arity, so a macro cannot generate one
statement for each item in a heterogeneous argument list. A fixed-arity
`raw macro` can name each resource explicitly, but it cannot scale to an
arbitrary number of resources. The motivating example is `@destroy(vec,
arena)`, which should register one cleanup statement per identifier and defer
those statements until the lexical block containing the call exits.

Size: language subsystem, macro declaration parsing, expansion, and cleanup
scope interaction in Zith--.

## Current Facts

- Zith-- supports `macro` and `raw macro` with fixed arity. Macros with the
  same name cannot be overloaded by arity.
- Function variadic slices such as `[...]T` collect homogeneous values. They
  do not provide syntax-level expansion for heterogeneous arguments.
- A `defer` registers cleanup with its nearest lexical block. Statements in a
  deferred block run in written order, while separate defers run in reverse
  registration order. See [scope cleanup](../09-control-flow.md).
- A `raw macro` that expands to `defer` currently associates cleanup with the
  macro expansion boundary. An observed example runs the cleanup before the
  next statement after the macro call. The target behavior is different:
  cleanup belongs to the lexical block containing the call.

## Proposed Surface

Both normal and raw macros may declare at most one variadic parameter, and it
must be the final parameter. The initial variadic argument kind is
`identifier`. Its tail may contain zero or more identifiers. Call syntax
remains an ordinary comma-separated argument list.

The postfix `...` on a template statement repeats that statement once for
each captured identifier, in argument order. The `destroy` use case is:

```zith
raw macro destroy(names: identifier...) {
    defer {
        names.destroy()...;
    }
}

@destroy(vec, arena);
```

The expansion registers one deferred body at the call site. That body contains
`vec.destroy()` followed by `arena.destroy()`. Separate generated or
user-written `defer` statements retain the existing reverse registration
order.

This is a proposed contract, not implemented syntax. The existing macro
surface is documented in [Macros](../15-macros.md).

## Non-goals

- Does not change function variadic slices or add a general-purpose spread
  operator.
- Does not accept expressions in the initial variadic tail. Arguments are
  identifiers only.
- Does not change the cleanup order of separate `defer` statements.
- Does not add automatic resource destruction, ownership rules, or a
  `destroy` keyword. `@destroy` is a macro use case.
- Does not add thread-specific macro behavior. This is a general Zith--
  language capability, independent of [`soon/thread`](stdlib/soon-thread.md).
- Does not implement the feature in this drawing.

## Open Decisions

- Whether postfix repetition applies only to expression statements initially
  or to every statement form.
- How macro expansion preserves the caller's lexical cleanup scope through
  nested expansions, control-flow exits, and imported macros.
- Which diagnostics cover a variadic parameter used outside a repeated
  statement, or used with an unsupported argument kind.
- Whether later iterations should allow variadic argument kinds beyond
  `identifier`.
