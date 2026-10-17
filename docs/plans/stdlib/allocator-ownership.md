# Standard Library Allocator Ownership

Status: drawing

This drawing standardizes explicit allocator ownership across the standard
library. Every resource-owning constructor receives an allocator explicitly,
retains the allocator identity, and uses that same instance to allocate, grow,
and free its storage. The migration covers all current stdlib allocations,
including `InputLine`, `FormatBuffer`, hash maps, and temporary buffers.

Size: system, the standard-library allocation surface.

Non-goals:

- `new`, `delete`, `make`, and `release` remain outside this work.
- `std/fs` remains deferred.
- Automatic destruction and language ownership semantics do not change.
- Legacy compatibility is not a constraint; modules may be changed or removed.

Open decisions:

- Validate how an owning value can retain and repeatedly use its allocator.
  A stored `dyn Allocator` passed `zithc check` but failed under `zithc run`
  with `E5001 Invalid InsertValueInst`.
- Audit allocation call sites and settle the migration order after the
  allocator representation is proven.
- Verify the allocator growth mechanism can preserve the owner's prior state
  when allocation fails.
