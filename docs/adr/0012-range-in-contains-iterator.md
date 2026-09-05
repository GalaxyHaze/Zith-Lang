# Range `in`/Contains and Separated Iterator

## Status

Accepted and implemented in the Zith-- compiler.

## Context

The subset previously used `..` only as a slice and `when` pattern shorthand.
The language needs a membership test (`value in range`) and a loop protocol for
user iterators without conflating membership with iteration.

Early design options treated `in` as sugar for iteration or transformed range
bounds before lowering. Both choices fail: membership needs `bool` over `lo/hi`
without altering bounds, and user iterators require `next(self): ?T` rather than
a containment method. There is also no reason to restrict membership to ranges,
so the protocol should be duck-typed and available to any type with a
`contains(self, value): bool` method.

Example:

```zith
struct Set {
    a: i32,
    b: i32,
    fn contains(self, value: i32): bool {
        return (value == self->a) or (value == self->b);
    }
}

fn main(): i32 {
    let s: Set = Set { a: 2, b: 9 };
    if (3 in s) { return 1; }
    return 0 in 1>..<4;
}
```

## Decision

`in` is a normal binary operator that returns `bool`. It resolves membership
through the `Contains` protocol `contains(self, value): bool`; a literal range
also satisfies `Contains` and lowers to bound comparisons.

Range literals keep raw bounds. The AST/HIR record `openAtLo` and `openAtHi`
for `lo>..hi`, `lo..<hi` and `lo>..<hi` instead of storing adjusted values.

`Iterator` remains the existing `next(self): ?T` loop protocol used by
`for (x in iterable)`. Literal integer ranges are an additional direct loop
path with implicit step `1`; float ranges are valid membership ranges but
are rejected for iteration.

## Consequences

- `x in range` works without allocating an iterator and keeps the same
  semantics in `if` and in a `when` subject.
- User-defined types can opt into membership independently of iteration.
- The compiler does not add or subtract `1` from bounds, so diagnostics and
  lowering stay faithful to the source range.
- Ranges and the existing slice syntax keep separate AST nodes; slice/`when`
  compatibility is preserved.
