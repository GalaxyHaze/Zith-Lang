# `soon/random` Random Generator And Uniform Distribution

Status: drawing

The standard library has no random-number source. Programs that need sampling,
shuffling, or simulation reach for host entropy with no portable surface and no
stated contract for seeding, reproducibility, or bias. The proposed surface is
a generator built by `random()`, `random(seed)`, `random(algorithm)`, or
`random(algorithm, seed)`, with a uniform sampler `dist<T>(lower, upper)`.
`dist<T>` is generic from the first version and requires a sampling trait. Its
first implementation is `i32`. This drawing records an experimental contract
for the generator, seeding, reproducibility, and range behavior.

`algorithm` selects the pseudorandom algorithm and defaults to
`Xoshiro256PlusPlus`, a non-cryptographic choice intended to balance
performance and statistical quality. A seed omitted by the caller is requested
from the operating system. If that fails, initialization uses available time
and process information, including PID when available, with basic mixing. This
fallback is best-effort, not cryptographic. If no usable source is available,
construction fails. An explicit `u64` seed is expanded deterministically with
SplitMix64.

Size: module, the standard-library random surface.

## Current Facts

- Zith-- has positional parameter defaults but no `opt` keyword. The original
  `opt` notation only described omitted arguments. The proposed API uses
  overloads and adds no parameter syntax.
- There is no named-argument call syntax. The seed-only form is a separate
  overload, distinguished from the algorithm overload by parameter type.
- Generic free functions and generic methods with trait bounds are working
  (`fn get<T>(self: view DynArray<T>, index: u64): ?T`, `T: Trait`), so
  `dist<T>` with a `T: Uniform` bound is expressible.
- The `soon/` policy in [soon promotion](soon-promotion.md) says new modules
  start under `soon/` and move to `std/` only after the stability gate.
  `soon/` is not on the default include path.
- There is no entropy binding in `c/` or `std/`. `getrandom`, `rand`, and
  `/dev/urandom` are all absent from the shipped surface.
- The current `c/` and `std/` surfaces do not expose a PID or a clock source.
  The fallback will need platform-specific bindings or another runtime source.
- The seed must work on all three backends, and none has entropy today. Native
  links libc but has no binding. VM v2 only accepts a validated extern set of
  17 names (`puts`, `malloc`, `memcpy`, ...), so a new source needs a VM
  handler. WASM stubs `random_get` to return `52` (unsupported) and
  `clock_time_get` to a fake deterministic counter. WASM therefore needs a
  host operation that supplies a seed.
- xoshiro256++ requires `u64` state operations. The VM needs `u64`
  wraparound arithmetic, logical shifts, and rotations. The VM work is a
  prerequisite for a matching implementation.

## Proposed Surface

The overload set and initial algorithm are decided. Exact declaration syntax
and error type names remain illustrative.

```zith
import soon/random as rnd

enum Algorithm {
    Xoshiro256PlusPlus,
}

fn main() {
    let seed: u64 = 42;
    var a = rnd.random(); // OS seed, then best-effort fallback
    var b = rnd.random(seed);
    var c = rnd.random(Algorithm.Xoshiro256PlusPlus);
    var d = rnd.random(Algorithm.Xoshiro256PlusPlus, seed);
    let die: i32 = d.dist<i32>(1, 7);
    var other = d.split();
}
```

`algorithm` is not an entropy source. The overloads are `random()`,
`random(seed: u64)`, `random(algorithm)`, and `random(algorithm, seed: u64)`.
Seeded construction returns a generator directly. Unseeded construction can
fail if neither the operating system nor the best-effort fallback supplies a
usable seed, so it returns a result.

For a given algorithm version and explicit seed, native, VM, and WASM targets
must produce the same sequence. This does not promise sequence stability across
algorithm or library versions.

The generator owns its state per instance. `Copy` duplicates the current state
and therefore the subsequent sequence. `split()` is the operation for a
separate stream: it advances the original generator and returns another
generator. `clone()` is not the mechanism for creating a separate stream.

## The Trait Seam

`dist<T>` is generic from the first version and accepts only types that
implement a sampling trait. The first implementation is `i32`. The trait maps
raw generator bits to values in range without modulo bias. The trait name and
its exact method contract remain open. Floating-point sampling and additional
integer types are deferred.

`dist<T>(lower, upper)` samples from the half-open interval `[lower, upper)`.
When both bounds are equal, it returns that value. When `lower > upper`, it
returns `Result<T, RangeError>` with an error. The `RangeError` name is
illustrative. Sampling the entire `i32` domain is deferred.

## Non-goals

- Does not add `opt`, named arguments, or a new parameter syntax. Trailing
  defaults are not used as a substitute for the overload set.
- Does not promise cryptographic security. The default algorithm is explicitly
  non-cryptographic.
- Does not add a cryptographic generator such as ChaCha20, HMAC-DRBG, or
  AES-CTR-DRBG.
- Does not add other distributions such as `normal`, `exponential`, or
  weighted choice in the first cut.
- Does not add shuffle, sampling without replacement, or collection helpers.
- Does not add floating-point sampling or types beyond the initial `i32`.
- Does not promise cross-version sequence stability.

## Open decisions

- The exact platform APIs for OS entropy, fallback time and PID, and the WASM
  host operation that supplies a seed.
- The exact `Uniform` trait name and method contract, plus the unbiased
  sampling method used for `i32`.
- The precise API spelling and error type names for seed acquisition and
  invalid ranges.
- The implementation of `split()` and its guarantees for stream separation.
- The algorithm's full-domain `i32` sampling behavior. The initial half-open
  range contract does not provide a way to express the entire `i32` domain.
