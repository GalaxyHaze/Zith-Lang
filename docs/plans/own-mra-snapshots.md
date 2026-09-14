# Own + MRA Snapshots

## Objective

This draft tests the conceptual syntax and usage for the future `Zith` memory
model before it is written into `docs/Zith-spec-full.md` or `docs/mra-spec.md`.
It deliberately keeps `Zith--` out of scope: the current compiler continues to
implement a simpler version, and anything here is an accepted design candidate,
not an implementation contract.

Decisions fixed before this draft:

- `own T` is the rename of `unique T`. It is a mutable single-owner handle.
- `&x` literally takes the address of a value. It makes the name `x` dead, but
  views/belongs that already exist remain valid because the resource is alive.
- A stack-based `own` may not escape its storage scope; returning it is an
  error because the backing storage is invalidated.
- Regions are MRA concepts and `own` remains an NRA concept. Region provenance
  is not embedded in the `own` type syntax.
- Region access uses array-like syntax as the primary conceptual surface.
- Dynamic overlap is proved by a mix of compile-time proofs and a conservative
  runtime creation check, favouring origin-based disjointness.
- Ptr-like results from `&region[index]` are `own T`, not a separate `Ptr<R>`.
  This keeps address-of and ownership transfer predictable across regions.
- `lend`/`view`/`belong` stay NRA-only and are not replaced by `own`.
- A moved field of a struct may be restored with an explicit re-binding
  operator rather than plain assignment, so the language cannot hide a double
  ownership write.
- `:=` is the general re-binding operator for handles and links. It updates an
  existing `own`, `share`, `lend`, `belong`, or `view` binding without treating
  the update as a value copy. It is not used for declaration initialization.
- Moving a non-primitive out of a region invalidates the specific slot, not
  the whole region or block.
- `let`/`var` control only whether the binding can be re-bound. Content
  mutability comes from the type: `mut T` makes fields mutable, a plain `T`
  stays immutable unless the owner is explicitly mutable.
- Struct fields can carry the bind prefixes as per-field overrides: no prefix
  follows the owner's mutability, `let field` is always immutable, and
  `var field` is always mutable even through an immutable owner.
- `meta` is a field/type prefix for values that live only in comptime/type
  evaluation and have no normal runtime storage. It is not promoted to a bind
  keyword.

## Binding And Field Mutability

`let` and `var` define re-bindability only. They do not decide whether fields
are writable; `mut T` does that for content, and a plain `T` is immutable.

```zith
var p: Point;        // p can be reassigned; Point fields are immutable
var p2: mut Point;   // p2 can be reassigned; Point fields are mutable
let q: Point;        // q cannot be reassigned; Point fields are immutable
let q2: mut Point;   // q2 cannot be reassigned; Point fields are mutable
```

The same binding model applies to struct fields as explicit overrides. Without
a prefix the field follows the owner; `let field` pins immutability, and
`var field` pins mutability even when the owner is immutable. This gives
`var field` the same role that `mutable` has in C++.

```zith
struct Counter {
    value: i32,       // follows owner mutability
    let id: u64,      // always immutable, even in a mutable owner
    var scratch: i32, // always mutable, even in an immutable owner
}
```

`var field` is the only deliberate exception to deep mutability. It lets a
specific storage slot change through a path whose outer value is otherwise
immutable. Readonly borrows such as `view` must still block the write; the
escape is per-field/owner, not an automatic bypass of NRA.

`const` and `global` remain storage/phase prefixes in structs, not content
mutability markers:

```zith
struct Config {
    meta count: i32 = 0,   // comptime-only, no normal runtime storage
    const VERSION: i32 = 3, // final constant
    global pool: Arena,     // global storage
    var scratch: i32,       // mutable even through an immutable owner
    let id: u64,            // immutable even through a mutable owner
}
```


## Region Declaration

The declaration names a region and authorizes a value range as usable memory.
This is intentionally drawn as a simplified surface; `heap` and `pool` remain
internal/derived forms when they are needed.

```zith
region VGA = 0x04000000..0x0400FFFF:
    access: read write
    init: true
    stream: typed u8

region Scratch = unknown..64 KiB:
    access: read write
    layout: bump

region OsHeap = dynamic:
    access: read write
```

Open syntax questions for the next iteration:

- Whether `init: true` stays a region property or becomes an initialization
  intrinsic.
- Whether `region Scratch = unknown..64 KiB` is the best spelling for a
  runtime-backed region.
- Whether `heap`/`pool` deserve dedicated keywords or remain derived MRA
  declarations.

## Static Region Access

Array-like access is the primary surface. A primitive read is a copy; a
non-primitive element follows ownership rules.

```zith
let cursor: u8 = VGA[30];
VGA[30] = cursor + 1;
```

`&` keeps address-of meaning and is the way to obtain a pointer-like value with
region provenance.

```zith
let oneChar = &VGA[30];      // own u8 if read is copy; address of slot 30
```

For the first draft, pointer arithmetic is not proposed. The useful operations
are index access, forming an owner from one element, and passing/returning the
owner with region provenance. Calling `&` on that owner keeps address-of
meaning and transfers ownership again.

## Copy And Move From A Region

A primitive read is a copy and does not invalidate the slot.

```zith
let a: u8 = VGA[30];   // copy, VGA[30] remains readable
```

A non-primitive element can be moved out of a region only when ownership of the
slots is known.

```zith
region RingBuffer = 0x1000..0x11FF:
    access: read write
    stream: typed Packet

let packet = &RingBuffer[3];   // ownership transfer of slot 3
```

`&` makes the result an `own T`, not a bare pointer, so the ownership result is
predictable. The move transfers ownership of the slot and the slot becomes
unavailable for normal access unless an explicit reuse/reset path permits it.
Only that slot is invalidated; other slots and the region remain usable.

## `own` As The Single-Owner Handle

`own T` is not the same as `unique T` only by name in the narrative: it is the
same NRA concept, expressed as a single-owner handle.

```zith
let file: own File = acquireFile();
read(file);              // own passed as default/borrow per NRA rules
drop(file);              // transfer to cleanup; file is dead
drop(file);              // ERROR: use after move
```

The handle can be stack-backed.

```zith
let cell: var i32 = 40;
let handle: own i32 = &cell;
// cell is dead after this line for normal reads.
// Existing view/belong references to cell remain valid.
```

Returning a stack-backed handle is an error.

```zith
fn makeStackOwn(): own i32 {
    let x = 41;
    return &x;   // ERROR: backing storage escapes its scope
}
```

## Struct Fields

`own` has different field semantics from a plain owned value. Taking an `own`
field from a struct moves that field and leaves the struct incomplete; filling
it again would overwrite another live resource.

```zith
struct Pair {
    left: own Buffer,
    right: own Buffer,
}

fn takeLeft(pair: own Pair) -> own Buffer {
    return &pair.left;   // field ownership transferred; pair is partial
}

fn repair(pair: ref Pair, buffer: own Buffer) {
    pair.left := buffer;   // explicit restore of an already-own field
}
```

Plain assignment to a moved field stays rejected because it would overwrite a
live owner. `:=` is the explicit form that tells NRA the old slot is dead and
installs a new owner. A partial struct remains readable in its unmoved fields;
the moved field is unavailable until restored.

Repair of a `default` field is different. If the source field is `default` and
it is taken through `&` / `own`, the field is not merely "moved but restorable":
its address was taken and the original slot is logically invalid. The owning
struct cannot fill that slot in place, because doing so would overwrite the
storage that another owner now references.

```zith
struct Records {
    tag: i32,
    payload: Buffer,   // default field
    next: own Records,
}

let owned = &records.payload;   // payload is logically invalid
records.payload = Buffer {};    // ERROR: cannot repopulate the taken slot
```

For a `default` field taken as `own`, the safe rule is to invalidate the struct
path and require the programmer to rebuild the struct through a fresh value:

```zith
let rebuilt = Records {
    tag: records.tag,
    payload: Buffer {},
    next: records.next,
};
```

The rebuild expression reads the fields that are still valid and explicitly
supplies a new `payload`. This preserves the invariant that a taken slot is
never written through an alias of the old struct; the new `payload` lives in a
different storage path.

## `:=` As The Re-binding Operator

`=` assigns values; `:=` re-binds the reference or ownership slot itself.
Its purpose is to make the distinction visible in source: writing a normal
assignment to a dead/full slot would look like a value copy, while `:=` states
that the existing slot is being retargeted or refilled.

The general rule is:

- `=` is allowed only when the target is alive and the operation is a value
  copy or a plain value store into an existing live slot.
- Declarations and initial initialization use `=`, not `:=`. `:=` is reserved
  for rebinding an existing slot or restoring a slot that became dead.
- `:=` is required when the slot was logically invalidated, when the target is
  a reference link that is being retargeted, or when ownership is installed
  into a slot that already has a type/lifetime contract.
- A `:=` on `own` installs a new owner into a slot that is known dead; it never
  overwrites a live owner.

### `own` And `own` Fields

```zith
var slot: own Buffer = acquireBuffer();  // OK: `=` initializes
slot := acquireBuffer();      // ERROR: live owner must move out first

let slot2 = &slot;            // slot2 now owns the storage
slot := acquireBuffer();      // OK: old storage is now owned elsewhere
```

A moved `own` field is restored the same way.

```zith
fn repair(pair: var Pair, buffer: own Buffer) {
    pair.left := buffer;   // old field owner was moved out; install new owner
}
```

### `share` Links

`share` names refer to a shared node without ref-counting. `:=` retargets the
name to a different shared node.

```zith
var a: share Config = load();
var b: share Config;
b := a;                    // b now refers to a's shared node
b := load();               // b now refers to a different shared node
```

### `lend` Links

`lend` is an exclusive temporary. Re-binding changes which node the temporary
points at; it must not transfer or duplicate ownership.

```zith
fn rebind(selector: i32) {
    var p: lend Frame;
    p := lend first;       // exclusive borrow of first
    p := lend second;      // borrow ends on first; second is now lent
}
```

This form is only valid in a mutable binding or reference slot. A `let`-bound,
non-mutable `lend` cannot be re-bound. The source node must be alive when the
borrow starts, and the previous borrow must end before the new one starts.

### `belong` Links

`belong` is a back-pointer whose lifetime is tied to a parent. `:=` retargets
the back-pointer to another valid parent; NRA still enforces the normal escape
and parent-alive rules.

```zith
node.parent := newParent;   // OK: both parents alive, old belonging ends
```

### `view` Links

`view` is a read-only reference. `:=` points it at another live value; it does
not change mutability or introduce ownership.

```zith
var config: view Config = load();
config := view other;       // now points at other
config := view third;       // now points at third
```

`:=` never creates ownership where none existed, and never removes an owner.
It only changes which node a link points at or installs an owner into a slot
that is awaiting one.

## `lend` / `view` / `belong` Do Not Change

The other memory qualifiers keep the existing conceptual meanings.

```zith
fn update(p: lend Frame) { p.cursor = p.cursor + 1; }
fn inspect(p: view Frame) -> u64 { p.cursor }

let frame = &store;          // move-style ownership handle
update(lend frame);          // passes exclusive borrow
inspect(view frame);         // passes read-only borrow
```

`own` and `&` do not alter how `lend`/`view`/`belong` interact at the ownership
layer.

## Dynamic Regions

A dynamic region may be backed by runtime memory. The declaration authorizes
the domain; initialization binds a concrete range.

```zith
region Scratch = dynamic:
    access: read write

let scratch = initRegion(Scratch, mmap(64 KiB));
let slot = &scratch[8];
```

For this draft, the runtime form is an arena-like object.

```zith
struct Arena<R> {
    base: usize,
    cursor: usize,
    size: usize,
}
```

`R` is the static MRA domain. The arena itself is ordinary runtime state; MRA
keeps proof metadata about the domain.

## Dynamic Overlap: Three Layers

MRA does not need to prove every dynamic region against every other one. The
proof model is layered, and each layer shifts the burden off the proof system.
The user-facing rule is: prove the smallest layer that covers the doubt; only
escalate to the next layer when there is real ambiguity.

### Layer 1: Static

`region` and `pool` declarations carry their `base`/`size` at compile time.
MRA treats them as the canonical reference and never revisits disjointness
between static regions: if two static regions overlap, the program is rejected
without runtime probing.

### Layer 2: Fresh

Memory obtained from the operating system through a fresh source is accepted as
disjoint by construction. The OS guarantees that a fresh mapping does not
overlap any prior allocation owned by the process; MRA treats that as a proof
premise without auditing the returned address.

```zith
let a = Arena::fresh(64 KiB);   // fresh origin, accepted disjoint
let b = Arena::fresh(128 KiB);  // no comparison needed
```

This puts the work in the OS/runtime, where the guarantee actually lives,
instead of in a global compile-time address checker.

### Layer 3: Slices

When more than one arena is carved out of the same backing range, MRA falls
back to slice proof. NIA/RRA must prove that the slices are contained in the
parent range and disjoint from each other. Runtime checks exist only at the
point of carving, never per access.

```zith
let parent = mmap(256 KiB);
let front  = parent.slice(0, 64 KiB).init<Scratch>();
let back   = parent.slice(192, 64 KiB).init<Scratch>();
// NIA/RRA prove disjoint 0..64 KiB and 192..256 KiB in one place.
```

This is the only layer where the proof is non-trivial. Static and fresh memory
bypass it.

### Why Not Prove Everything

Proving every dynamic region against every other region moves the work out of
where it belongs (OS origin or NIA/RRA inside one backing range). The
three-layer split keeps MRA focused on edges that actually need a proof:
same-origin slices. Cases outside the layers fall through to the conservative
MRA rule already recorded in the draft (`Overlap` until `@regionInit` proves
otherwise).

## Tooling Mapping

Only names that map cleanly without changing the current pipeline:

- `docs/Zith--.md` and `Zith--` compile behavior stay unchanged.
- `docs/Zith-spec-full.md` and `docs/07-memory-model.md` can replace the
  narrative uses of `unique` with `own` where they describe the full language.
- `src/frontend/frontend.hpp` already has `OwnershipKind::Unique`. Keeping the
  internal spelling avoids a broad rename while the surface name becomes
  `own`.
- `src/types/type-kind.hpp` has the same `OwnershipKind::Unique` internal name.
- MRA syntax from `docs/mra-spec.md` remains a candidate surface; this draft
  does not yet replace `@regionAt` with `VGA[...]` in implementation docs.

## Open Questions

- Region declaration syntax: keep `heap`/`pool` keywords or derive them from a
  single `region` declaration.
- What `Ptr<VGA>` operations are allowed outside raw/unsafe.
- How a partial struct with a moved `own` field is represented and repaired.
- How `init` forms a concrete `Arena<R>` from static/dynamic declarations.
- Whether `@regionAt` remains the implementation intrinsic while `VGA[...]`
  becomes sugar.
