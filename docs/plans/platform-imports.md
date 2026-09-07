# Platform-Specific Imports Roadmap

## Objective

Allow a project, dependency, or stdlib module to provide `OS`/`arch`-specific
behavior without forcing users to write conditional code inside modules. The
compiler keeps `import foo` as the only user-facing syntax; module resolution
selects a platform-specific file when one matches the active target.

This is a Zith-- feature: it belongs in the module/import resolver and does not
introduce comptime, macros, or new conditional statement syntax.

## Resolver Contract

For a Zith import `foo` and a normalized active target with `arch = <arch>` and
`os = <os>`, resolution checks the following candidates in order in the same
root already used for the import:

```text
1. foo.<arch>.<os>.zith
2. foo.<arch>.zith
3. foo.<os>.zith
4. foo.zith
```

Rules:

- `foo.zith` remains the required generic fallback. Its existence is not
  optional in a platform-specific resolution namespace.
- Partial variants are allowed: `foo.<arch>.zith` and `foo.<os>.zith` can match
  independently.
- The active target must match `arch` and `os` independently; `none`, unknown,
  or empty OS/arch values never match a literal target component.
- Only the last segment of a compound path gets variants, e.g.
  `foo/bar.<arch>.<os>.zith`.
- No platform-specific directories in v1, so `foo/<arch>.<os>/mod.zith` is not a
  candidate.
- Ambiguity between multiple matching files is intentionally not an error in v1;
  the resolver uses the first deterministic candidate. A future lint may report
  duplicates.

## Active Target

`resolveImport()` needs an effective active target identical to the target used
by codegen and C compilation:

- `--target <TRIPLE>` when provided.
- Otherwise `llvm::sys::getDefaultTargetTriple()`.
- When LLVM is unavailable, use `host` and do not generate platform variants.

Variant names use canonical `llvm::Triple` names, e.g. `x86_64`, `aarch64`,
`arm`, `linux`, `darwin`, `windows`. Aliases such as `amd64`, `arm64`, or
`macos` are not mapped in v1.

Vendor and environment stay out of v1. The candidate list is structured so a
future `foo.<arch>.<vendor>.<os>.<env>.zith` level can be added without changing
the user-facing import syntax.

## Cache

The existing `CacheKey` already includes `targetTriple`, so the module cache is
already target-separated. This is safe for v1 and needs no cache-specific code.

Documented future work: generic modules could share cached artifacts across
targets, but that requires splitting module-artifact caching from the
target-dependent resolution graph. It is deferred until the feature is stable.

## Diagnostics

When no candidate wins and no generic `foo.zith` exists, keep the existing import
error but make it actionable when platform variants were considered:

```text
could not resolve import 'foo'; missing generic module or matching platform variant
```

When a platform file is selected, module diagnostics, spans, and cache paths
point at the selected file (`foo.<arch>.<os>.zith`), never at the logical import
name.

## Export Semantics

`export foo` re-exports the resolved platform file for the current target. No
new export syntax is needed; the final resolved module path flows through the
existing import graph.

## Implementation Sketch

1. Add a small target-component helper, for example in
   `src/session/frontend-context.cpp`, that exposes:
   `arch` and `os` from the effective target, or `nullopt`/empty when LLVM is
   unavailable.
2. In `FrontendContext::resolveImport()`, after the literal candidate search and
   before `path.zith`, generate:
   `candidate.<arch>.<os>.zith`, `candidate.<arch>.zith`, and
   `candidate.<os>.zith`.
3. Keep the existing literal-path behavior so explicit imports and C headers are
   unaffected.
4. Keep cache lookup keyed by the final canonical path; the `CacheKey` already
   distinguishes targets.
5. Update the import failure message when variants were considered.

## Tests

Minimum coverage in `tests/test-frontend-context.cpp` and
`tests/test-frontend-modern-pipeline.cpp`:

1. Exact `arch.os` variant wins over generic.
2. Arch-only variant wins when it matches.
3. OS-only variant wins when it matches.
4. Generic `foo.zith` is chosen when no variant matches.
5. Missing generic and no matching variant produces the actionable diagnostic.
6. Two targets choose different files; cache behavior remains target-separated.
7. `export foo` propagates the resolved platform file.

## Docs To Update

- `docs/plans/0.7.0/README.md`
- `docs/roadmap.md`
- `memory/platform-imports.md`

After implementation, also update:

- `docs/Zith--.md`
- `docs/Zith---implementation.md`
- `docs/impl-status.md`

## Verification

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target fmt-check
```

Platform-import tests must pass without LLVM when they use only parser/resolver
behavior; LLVM-independent paths must not leak `llvm::Triple` into code compiled
without LLVM.
