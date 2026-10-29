# Local Package Resolution and Artifact Cache

Status: accepted

The Zith-- MVP consumes named, versioned packages from local paths. Package
identity uses the required non-empty strings `[project].name` and
`[project].version`, with versions treated as opaque identifiers. The
`[dependencies]` table maps each declared package name to a relative local path,
and `[package].module_root` optionally selects the package's module root
(default `src`). `deps resolve` records the transitive graph, resolved paths,
and manifest fingerprints in a versioned project lockfile while leaving source
contents live. `check` and `build` require a valid lockfile and never resolve
implicitly. Distributors provide ZIRL by default, and builds fall back to
included source, storing compiled artifacts in the project-configured local or
shared cache. The MVP keeps dependency selection separate from artifact reuse,
keeps transitive dependencies private unless re-exported, rejects cycles and
conflicts involving package names or module paths, and excludes remote
registries, version ranges, aliases, multiple versions, package build scripts,
and C/CMake packages.

The project lockfile is `Zith.lock`, encoded as deterministic TOML so users can
inspect it and the compiler can reject unsupported schema versions. Format
version 1 stores the project identity and lowercase SHA-256 manifest
fingerprint, followed by sorted direct dependency names and sorted package
records. Each package record stores its identity, project-relative normalized
path, package-relative normalized module root, manifest fingerprint, and
dependency names. Relative paths keep the committed lockfile portable across
checkout locations.
