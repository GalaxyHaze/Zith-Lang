# Platform-Specific Imports

## Summary

Implemented: `import foo` resolves platform variants such as
`foo.<arch>.<os>.zith` against the active compilation target while keeping the
generic `foo.zith` as a required fallback. The feature belongs in the module
resolver, not in comptime, macros, or a new conditional statement.

Full design: `docs/plans/platform-imports.md`.

## Current Ground Truth

`FrontendContext::resolveImport()` checks the requested path, platform suffix
variants, the generic `path.zith`, and `path/mod.zith` per search root.
`--target` reaches `FrontendConfig::targetTriple` and participates in
`CacheKey::identity()`, so the module cache remains separated per target.

## Decisions Locked In

- Syntax remains `import foo`; no conditional import syntax.
- File naming is `foo.<arch>.<os>.zith`.
- Resolution order: `arch.os` -> `arch` -> `os` -> generic `foo.zith`.
- Generic module is required; missing it with a non-matching target is an error.
- Variants are files only; no platform-specific directories.
- Compound imports vary only on the last segment, e.g. `foo/bar.<arch>.<os>.zith`.
- Canonical `llvm::Triple` names are used; no aliases such as `amd64`/`arm64`.
- Vendor and environment are excluded from v1; the candidate structure can grow
  to `foo.<arch>.<vendor>.<os>.<env>.zith`.
- Cache remains target-separated for now; sharing generic module artifacts across
  targets is documented future work.
- No ambiguity error in v1; first deterministic candidate wins.
- Missing Zith imports mention both the generic module and platform variants
  when platform candidates were considered.
- `export foo` re-exports the resolved platform file for the current target.

## Gotchas

- Parse happens before target-based resolution, so only the resolver changes;
  the frontend parsing path stays untouched.
- Without LLVM, target component extraction returns no components and no
  platform variants are generated; code is compiled without leaking
  `llvm::Triple`.
- C header imports are handled by a different branch and must not start matching
  Zith platform variants.
- The persistent/object cache paths already use target keys, so no new top-level
  cache layout is needed.
- The resolver uses `fs::is_regular_file` for variants and generic files, then
  `foo/mod.zith` for directory modules; only the last path segment varies.
