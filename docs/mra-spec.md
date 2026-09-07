# MRA Spec - Memory Region Analysis (Draft)

This document is a future-design spec for Memory Region Analysis (MRA). MRA
treats static hardware/virtual memory regions as first-class, array-like
regions with permissions and optional typed streams.

## 1. Scope

Safe Zith code does not permit arbitrary addresses. In `raw`/`unsafe`, an
address is only usable when it is contained in a declared memory region and
the requested operation is permitted.

MRA is not an ownership analysis and not a borrow checker. It provides region
shape, permissions, and access typing. RRA verifies the geometry and NRA
verifies the surrounding ownership context.

## 2. Static Regions

All memory regions are static declarations known to the compiler. There is no
runtime region table and no runtime lookup feature.

```text
region VGA:
  base:  0x0400000C
  size:  0xB8
  read:  true
  write: true
  init:  true
  stream: untyped
```

Candidate fields:

| Field | Meaning |
|---|---|
| `base` | Start address of the region. |
| `size` | Byte length of the region. |
| `read` | Reads are permitted. |
| `write` | Writes are permitted and imply read on the same access. |
| `init` | The region may be initialized once from `Uninit`. |
| `stream` | `untyped` bytes or `typed T` access. |

## 3. Region State

`init` is a state transition, not a persistent permission:

```text
Uninit --init--> Init
```

| State | Permitted |
|---|---|
| `Uninit` | `init` only. |
| `Init` | `read` and/or `write` according to the region declaration. |

Until the state model is formalized, overlapping regions with different
initialization states are rejected conservatively.

## 4. Typed And Untyped Streams

An untyped region is accessed as bytes:

```text
@writeRegion(VGA, *char, 0x0400000C..0x04000CFF);
```

A typed region is accessed through a declared element type:

```text
region Framebuffer:
  base:  0x04000C00
  size:  0x1000
  read:  true
  write: true
  stream: typed Pixel
```

Typed access advances in `sizeof(T)` steps and must be aligned/proven within
the region.

## 5. Access Intrinsics

The initial MRA surface is:

| Intrinsic | Meaning |
|---|---|
| `@readRegion(region, T, range)` | Read `range` as `T` from a static region. |
| `@writeRegion(region, T, range)` | Write `range` as `T` to a static region. |
| `@regionOf(expr)` | Return the static region name that contains a constant/proven address. |
| `@regionContainsRegion(range, region)` | Return whether one range is contained in a region. |
| `@regionBounds(region)` | Return `{base, size}` for a static region. |
| `@regionPermissions(region)` | Return `{read, write, init}` for a static region. |

These intrinsics resolve during compilation. A runtime lookup is intentionally
not provided because every region is static.

Example:

```zith
#writeRegion(VGA, *char, 0x0400000C..0x04000CFF);

let name = @regionOf(0x04000010);
let ok = @regionContainsRegion(0x0400000C..0x040000C4, VGA);
let base = @regionBounds(VGA).base;
let canWrite = @regionPermissions(VGA).write;
```

## 6. Ambiguity

If an address can belong to more than one declared region, `@regionOf` is a
compile-time ambiguity diagnostic. The programmer must make the region
explicit.

If RRA cannot prove that a dynamic access fits a single region, the access is
rejected unless it is explicitly marked `raw` with an opt-out that MRA and
NRA still understand.

## 7. Open Questions

- Exact declaration syntax for regions.
- Whether overlapping regions are allowed and how precedence is reported.
- Cursor semantics for typed streams.
- Runtime-safe helper functions over static regions.
- Whether `init` can be performed by more than one caller or only by one
  initialization site.
