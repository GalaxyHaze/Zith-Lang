# Standard Library `DynArray`, `Result`, and `panic`

Status: drawing

This drawing adds the essential allocator-backed `DynArray<T>` and a small
error-handling foundation to the standard library. `DynArray` is the primary
deliverable. Its constructors take an explicit allocator, `reserve` and `push`
report failure without changing the prior state, and access/removal return an
optional value. `cleanElements` explicitly cleans `InPlace` elements, clears
the length, and keeps capacity; call it before `destroy` when elements own
resources. `destroy` frees only the backing buffer.

The same expansion adds a single generic `Result<T, E>` union with named
`Ok`/`Err` construction, contextual inference, `Result<void, E>`,
`isOk`/`isErr`, and `unwrap`/`unwrapErr`. Unwrapping the wrong case panics.
`E` is unrestricted, payload formatting is not part of panic, and payload
cleanup remains the caller's responsibility. No `?` integration is included.
A public fatal `panic(message)` reports through the backend's diagnostic
channel and does not recover.

Size: module, the standard-library collections and error-handling surface.

Non-goals:

- Allocator standardization is tracked separately in
  [allocator ownership](allocator-ownership.md), and must land first.
- `new`, `delete`, `make`, and `release` remain outside this work.
- `std/fs`, automatic destruction, and `?` integration remain deferred.
- No language-wide ownership or error-propagation feature is added.

Open decisions:

- Validate `DynArray` layout, growth overflow, alignment, and failure behavior
  against the allocator contract.
- Probe the compiler support needed for named generic union cases, contextual
  inference, and `Result<void, E>`. Existing notes record blockers for generic
  `Ok`/`Err` helpers.
- Choose the portable backend mechanism that makes `panic` fatal while
  preserving its message in the available diagnostic channel.
