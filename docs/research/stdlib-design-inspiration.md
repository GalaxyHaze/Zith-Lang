# Standard Library Design: Go And Zig

**Access date:** 2026-10-31

This comparison uses the local Go 1.26.7 source, Go 1.27.3 package
documentation, and the official Zig 0.17.0 release source. It treats these
libraries as prior art, not as contracts to copy. Detailed source notes are in
[Go reference notes](stdlib-references-go.md) and
[Zig reference notes](stdlib-references-zig.md).

## What Transfers

| Area | Go reference | Zig reference | Direction for Zith |
|---|---|---|---|
| Module boundaries | Small packages divide strings, bytes, UTF-8, conversion, and I/O. Internal leaf packages break dependency cycles. | A public `std` facade re-exports curated modules while implementations live in submodules. | Keep leaf modules independently importable. Add a facade only when it removes real import friction. Avoid making `std` one large implementation module. |
| Text | `strings` and `bytes` have similar APIs because Go distinguishes immutable strings from mutable byte slices. | JSON and formatting work with byte slices and writers instead of requiring one universal string object. | Define byte-slice semantics first. Keep owned text distinct from borrowed `[]char`, and put UTF-8 decoding and validation in a separate module. Do not duplicate all string operations for an owning wrapper. |
| Allocation | Garbage collection hides most allocation and cleanup decisions from package APIs. | Allocators are explicit. Containers either retain an allocator or receive it on operations, and callers release owned storage. | Use Zig as the primary memory model reference. Decide per type whether allocator identity belongs in the owner. Verify the stored `dyn Allocator` path beyond its focused struct-literal runtime test before standardizing it. |
| Collections | Generic free functions in `slices` and `maps` provide algorithms without enlarging each container's method set. | `ArrayList` and maps document allocator pairing, deinitialization, growth, and pointer invalidation. | Combine both: container methods own stateful invariants, while generic algorithms operate on slices and map interfaces. Make failure atomicity and invalidation observable contracts. |
| Errors | A small `error` interface supports wrapping and classification. Panics remain separate from ordinary failures. | Allocation returns an explicit `OutOfMemory` error union. | Keep ordinary failures in Zith's `Result` or optional/status APIs. Reserve fatal panic for violated assumptions, not allocation failure. Consider structural cause access separately from the `Result` payload. |
| I/O | One-method `Reader`, `Writer`, and `Closer` interfaces compose into larger capabilities. Algorithms are free functions over those contracts. | `std.Io` separates an explicit backend context from buffered reader/writer interfaces. | Start with narrow reader/writer traits and free functions. Keep buffering and platform dispatch as separate layers. Do not import either library's complete I/O framework. |
| JSON | [Go 1.27](https://go.dev/doc/go1.27#standard_library) separates typed `encoding/json/v2` conversion from syntax-level `jsontext` tokens and raw values. | JSON separates scanner, typed parsing, dynamic `Value`, and writer-based serialization. | Keep syntax and typed conversion distinct. Settle tokens and errors, choose the owned dynamic value representation, then add parsing and writer-based serialization. Do not promise generic typed reflection before the compiler can support it. |
| Concurrency and staged work | Go starts goroutines with a [`go` statement](https://go.dev/ref/spec#Go_statements), with lifecycle and result collection handled separately. | [`Io.async`](https://github.com/ziglang/zig/blob/0.17.0/lib/std/Io.zig#L1296-L1327) returns a `Future` whose execution and cancellation depend on the I/O backend. | Do not add language-level `async` or implicit suspension as the default. Keep synchronous calls ordinary functions. Represent genuinely staged operations with explicit state and caller-driven transitions. |

Go's string/bytes duplication is a consequence of its built-in immutable
`string` and garbage-collected ownership model. It is not a reason for Zith to
create parallel packages before Zith has the same distinction. Zig is a better
reference for allocator ownership and growth guarantees. Go is a better
reference for small composable interfaces and generic algorithms.

## Zith Constraints

- The compiler has no exceptions or RTTI. The current library already has a
  `Result<T, E>` module, but `?` integration and `Result<void, E>` remain
  separate open work.
- `dyn Trait` works for dynamic method dispatch. Storing a `dyn` field has a
  focused runtime regression for struct-literal coercion, but the allocator
  drawing still requires broader ownership and growth validation.
- `state`/`jump` transitions are explicit and lower to direct tail calls. They
  do not implicitly suspend, schedule work, or create an asynchronous runtime.
- The library design direction is to prohibit `async` generally and represent
  staged progress with explicit state.

The last constraint is not a proposal to turn every I/O call into a state
machine. Synchronous operations should stay ordinary functions. An operation
that genuinely spans steps must expose its state and transitions so callers
can decide when to advance it. No future, coroutine, callback, or hidden
suspension should be required by the default API.

## Recommended Order

1. Finish the allocator drawing. Specify allocator identity, ownership,
   release responsibility, and the unchanged-state guarantee after failed
   growth. Test retained and per-operation allocator forms before choosing
   which is the default for owning containers.
2. Finish the error contract alongside allocator work. Use the existing
   `Result<T, E>` surface for recoverable failures. Keep `null` for absence,
   not as an overloaded allocation or parse error. Decide separately whether
   errors need cause chaining.
3. Define the text foundation. Make borrowed bytes explicit, define an owned
   string's lifetime and mutation rules, and keep UTF-8 decoding separate from
   byte indexing. Keep C null termination at C boundaries rather than making
   it an invariant of every Zith string.
4. Stabilize `DynArray<T>` and slice algorithms. Specify allocation failure,
   overflow, element cleanup, pointer invalidation, and allocator pairing.
   Build `sort`, `find`, and similar generic helpers over slices where possible.
5. Keep I/O small. Add read/write traits only when concrete modules need them.
   Layer buffering over those traits, and keep platform-specific file bindings
   under `c/` separate from safe owning wrappers.
6. Implement JSON in independently testable slices. Start with a bounded
   scanner and diagnostics. Add the dynamic `Value` only after owned strings
   and collection cleanup are settled. Add serialization to a writer before
   convenience APIs that allocate an output string.

## Promotion

Treat `soon/` as a place to test names, ownership, and error behavior, not as a
shorter path to `std/`. A module should move to `std/` only after its public
promises have contract tests, the supported backends agree, and ownership and
failure behavior are documented. A percentage such as "10% stable" is not
measurable until the project defines its denominator. Prefer explicit
compatibility evidence over a time-only threshold.

The next useful design work is to resolve the allocator and error promises,
then revise the `soon/string` and `soon/json` drawings against those decisions.
Do not start a broad implementation from the comparison alone.

## Sources

- [Go standard library notes](stdlib-references-go.md), based on the local
  Go 1.26.7 source tree and Go 1.27.3 package documentation.
- [Zig standard library notes](stdlib-references-zig.md), based on the official
  Zig 0.17.0 release source and download page.
- [Zith allocator ownership drawing](../plans/stdlib/allocator-ownership.md)
  and [`DynArray`/`Result` drawing](../plans/stdlib/dynarray-result-panic.md).
- [Zith control-flow documentation](../09-control-flow.md#94-state-functions-and-state-machines)
  and [implementation status](../impl-status.md).
