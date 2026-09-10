# NRA Spec - Future Zith Design

This document is the source of truth for the future Zith Node Resource Analysis
(NRA) design. It describes the intended `Zith` model, not the current `Zith--`
compiler behavior. `docs/impl-status.md` continues to describe what the current
toolchain implements.

NRA is one of the four `Zith Proof Kernel` (ZPK) sub-systems.

## 1. Scope

NRA proves ownership and lifetime safety before the final HIR is formed. The
analysis reads a derived effect-header for each function and applies the
declared effects to resource nodes at call sites. It never re-proves ownership
from LLVM or HIR.

The core contract deliberately keeps a small state machine:

- `alive`: the resource is ready for normal read and use.
- `dead`: the resource has been moved away or locked; it can only be replaced
  by a new binding, reassignment, or explicit restore.
- `lent`: the resource is temporarily borrowed; the owner cannot use it until
  the borrow ends.

`lend`, `view`, `unique`, `share`, `belong`, and `MultiShare` are permissions,
ownership groups, anchors, or transport wrappers. They are not extra states in
the proof state machine.

## 2. Effect Header

Every function has a derived effect-header. The compiler derives it from the
function body, validates it, and serializes it so cross-module calls do not need
to reanalyze function bodies.

### 2.1 Argument Effects

For each argument the header records:

| Effect | Meaning |
|---|---|
| `read` | The callee reads the argument value. |
| `write` | The callee writes through the argument. |
| `move` | The callee consumes or transfers the argument. |
| `borrow` | The callee obtains a temporary `lend` or `view`. |

### 2.2 Argument Origins

For each argument the header records enough origin information for the NRA to
apply effects correctly:

- root local;
- field path;
- temporary;
- literal;
- call result;
- heap/allocator provenance.

`argEffects` and `argOrigins` are stored together. Origin is not user syntax in
this design; it is derived compiler metadata.

### 2.3 Return Provenance

The header records how the return value relates to owned resources:

- `returnsArgument(i)`;
- `returnsNewOwned`;
- `returnsBorrow(i)`;
- `returnsView(i)`.

Calls whose return can branch between several provenances use the conservative
union of the possible return facts.

### 2.4 Internal Effect Facts

The following facts are internal to the NRA and are not part of the public
effect-header:

- `capture`;
- `escape`;
- `alloc`;
- `free`;
- `fork`;
- `merge`.

For `extern fn` and other interop boundaries the public effect-header may be
provided through an explicit attribute. The exact attribute form is a follow-up
for the interop design.

## 3. NRA Application

The NRA consumes effect-headers and mutates the call-site resource graph:

1. Apply argument effects to the corresponding resource nodes.
2. If the callee borrows an argument, mark the owned resource as `lent` for the
   call duration, or create an anchor for the returned relationship.
3. If the callee moves an argument, move the logical ownership and mark the
   original binding `dead`.
4. Attach `returnsBorrow(i)` or `returnsView(i)` as an anchor chain from the
   argument node.
5. Record the transfer and actual result provenance for the caller.

When a function has a cached effect-header, the call site uses that header. The
NRA does not scan the complete callee body again for normal calls.

### 3.1 Return Alias Optimization

`let y = f(x)` where the header says `returnsArgument(i)` can be represented in
the resource graph as `y == x`, after NRA validation. This is an optimization
boundary, not a relaxation of ownership rules.

## 4. Resource Nodes

Each storage origin and each resourceful value is represented by a resource
node:

| Field | Meaning |
|---|---|
| `origin` | Stack, heap/allocator, literal, static, or temporary. |
| `ownerEdges` | Edges that participate in destruction/cleanup rights. |
| `anchorEdges` | Edges that keep the resource alive while they live. |
| `target` | Optional transfer target after a move. |
| `allocationSite` | Origin of the storage and its guardian. |

The exact in-memory layout is an implementation suggestion, not a mandated ABI.
The spec fixes the semantic edges, not the data structure.

## 5. Ownership Modifiers

| Modifier | Contract |
|---|---|
| `default` | Owned by the binding. Lifetime follows the resource graph. |
| `lend` | Exclusive mutable temporary borrow. The owner cannot use the resource while it is lent. |
| `view` | Read-only anchor. A view never owns, never destroys, and never promotes stack storage to heap. |
| `unique` | Logical ownership of an address/slot. Moving a unique value marks the source binding dead. |
| `share` | Owner group. Several static owners may coexist; cleanup happens when the last owner edge ends. |
| `belong` | Child/parent lifetime edge. It cannot escape its parent and can be passed as `lend`. |

`view` is called an anchor, not a weak owner. Anchors can delay cleanup, but
they cannot destroy and they never imply storage ownership.

## 6. Rule 1 - Argument Exclusivity

In a call, mutable and immutable access to the same resource cannot be mixed.
Passing the same binding as `lend` plus `view`, `lend` plus `lend`, or similar
conflicting access in one call is an ownership error.

Passing the same resource as several `view` arguments is allowed and emits a
warning. It is rejected only by the policy that makes the exclusive-access rule
uniform; it is not evidence of unsafety by itself.

## 7. Move Semantics

### 7.1 Physical Move

`let y = x;` is a physical move of the resource graph. For a struct, the whole
graph moves: `x` becomes `dead` until it receives a new resource. Fields that
are resourceful move with the struct and keep the same provenance.

### 7.2 Logical Move

Taking a `unique` field or a reference/`view` to a slot creates a logical move
of that address. The original name may become `dead` for that slot even when
other fields are still usable. Replacing the slot without returning the taken
resource would overwrite another resource and is an error. The owner must
return the original resource or replace the whole struct/binding.

### 7.3 Reassignment With Anchors

If a `view` anchors an old resource, `x = newValue` does not delete the old
resource immediately. The old resource stays alive as long as the anchor lives.
The anchor only delays lifetime; it does not add destruction capabilities.

### 7.4 Raw And Dead State

`raw` is not an escape hatch for dead or uninitialized state. Reading a resource
that is `dead`, or accessing an uninitialized variable, remains invalid even
through `raw`. `raw` may opt out of other non-core analyses, but it does not
bypass the basic dead-state rule.

## 8. Control-Flow Merge

Branches analyze local facts independently. At a join point, a binding is
`alive` only if it is `alive` on every control path that reaches the join.

Function return facts follow the same rule: when paths disagree, the analysis
uses the conservative union rather than assuming a path.

## 9. Cleanup And Allocation

Cleanup follows this order:

```text
fail -> defer -> drop -> storage free
```

- `fail` handles the failure/error path.
- `defer` runs registered scope cleanup.
- `drop` destroys the logical contents of the value.
- Storage free is the final step and is always the responsibility of the
  guardian/allocation site.

`drop` and `defer` can postpone storage free and keep the resource alive, but
they do not change resource identity. Storage is freed after the NRA proves
destruction and the allocation/origin is still valid.

`Allocator` is a capability. The compiler understands allocation through this
capability, but `unique` and `share` do not necessarily allocate: their storage
may come from the stack or from a temporary.

In the future Zith model, an `Allocator` is parameterized by a comptime MRA
region/heap/pool:

```text
capability Allocator(R):
    fn alloc(self, size: u64, align: u64): Ptr<R>!
    fn free(self, mem: Ptr<R>, size: u64, align: u64): unit!
    fn realloc(self, old: Ptr<R>, old_size: u64, old_align: u64,
               new_size: u64, new_align: u64): Ptr<R>!
```

`Ptr<R>` carries allocation provenance. NRA uses that provenance to reject
`free`/`release` from a different allocator and to decide whether a block is
`alive`, `dead`, or still anchored. MRA provides the region identity and
permissions; it does not prove which block is owned.

Dynamic heaps are allowed. `heap OsHeap` has `size: dynamic`; the useful bounds
are recorded on `Block<OsHeap> { ptr: Ptr<OsHeap>, len: u64 }`. NRA still
verifies ownership/lifetime for those blocks without MRA pretending that the
heap has a static total size.

## 10. MultiShare And Fork

`MultiShare<T>` is both a capability and a compiler-provided wrapper/type.
Every type has a default conformance; specific types may override it.

- `MultiShare` is only meaningful around a thread fork.
- Outside a fork with `forkCount == 0`, it is an invisible transport layer.
- `backend fork Entry(share value)` creates `MultiShare<T>` with `forkCount = 1`
  and moves the payload out of the parent scope.
- While `forkCount != 0`, the parent cannot read or write the original binding.
- `merge` restores the parent binding and returns the entry result type, not
  `MultiShare<T>`.
- Without `merge`, the resource remains with the fork.
- When `forkCount == 1`, stack payloads are rejected. The compiler requires a
  heap/relocated payload or proves the frame outlives the branch.
- `ownerThreadId` exists only on the promoted transport; normal `share` values
  do not carry ownership metadata.

`MultiShare` is the only place where the near-static share model allows a
threading edge case to be checked explicitly.

## 11. Cache And Backend Boundary

- The effect-header is serialized and reused by the cache.
- Resource node layout is an internal memory representation and may be rebuilt
  from headers.
- HIR and LLVM are not sources of truth for ownership.
- Backend attributes such as `readonly`, `nocapture`, and `noalias` are derived
  hints from validated NRA facts.

## 12. Follow-Up Contracts

The following parts need separate focused documents before implementation:

- explicit `extern fn` effect-header attributes;
- custom `Allocator` details, region parameterization, and storage semantics;
- full diagnostic catalog and accepted/rejected examples;
- precise interaction of `fail`, `defer`, `drop`, and storage free for every
  control-flow case;
- `belong` lifetime rules for field destruction and parent replacement.

Numeric interval facts, region geometry, and memory-region permissions are
specified separately in:

- `docs/nia-spec.md` for NIA facts and headers;
- `docs/rra-spec.md` for region relationships and geometric bridging;
- `docs/mra-spec.md` for static memory regions and raw access intrinsics.
