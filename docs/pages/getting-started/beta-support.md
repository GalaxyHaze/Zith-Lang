---
id: getting-started-beta-support
title: Beta Support And Limitations
section: Getting Started
output: getting-started/D-beta-support.html
aliases: intro/D-beta-support.html
kind: editorial
---
# Beta Support And Limitations

Zith-- is in beta. This table states what was tested, what was not, the user
impact, and the workaround for each affected platform or feature. It is the
support claim of record for the beta. Treat anything not listed as supported as
a candidate or unverified.

This path assumes you are new to Zith-- but comfortable with a terminal, build
tools, and basic systems-language ideas such as types, functions, imports, and
explicit resource management. It does not assume any compiler-implementation
experience.

## Platform matrix

| Platform and architecture | Artifact built | Selection mocked | Native smoke test | End-to-end check | Beta claim |
|---|---|---|---|---|---|
| Linux x86-64 (GNU runtime) | yes | yes | yes | yes | **Supported** |
| Linux AArch64 (GNU runtime) | yes | yes | no | no | Candidate, unverified |
| Windows x86-64 | yes | yes | no | no | Candidate, unverified |
| Windows AArch64 | yes | yes | no | no | Candidate, unverified |
| Linux musl | yes | yes | no | no | Outside scope, unsupported |
| macOS (x86-64 and AArch64) | yes | yes | no | no | Outside scope, unsupported |

The compiler release workflow builds artifacts for Linux, macOS, and Windows.
An artifact build and a mocked OS/architecture selection test are not native
runtime or end-to-end support. Only Linux x86-64 with the GNU runtime passes the
installation route and the documented check path end to end in this beta.

## Known limitations

| Area | What was tested | What was not tested | User impact | Workaround |
|---|---|---|---|---|
| macOS | Artifact build only | Native install and check | No supported macOS install | Use Linux x86-64, or build from source and treat the result as unverified |
| Linux musl | Artifact build only | Native install and check | The musl binary and language server are not verified | Use the GNU runtime |
| Windows | Artifact build and mocked selection | Native install and check | No supported Windows install | Use Linux x86-64, or build from source and treat the result as unverified |
| Linux AArch64 | Artifact build and mocked selection | Native install and check | No supported AArch64 install | Use Linux x86-64 |
| `c/math` | Type check | Native link on every target | A program calling `sin`/`fabs` may fail to link when the host does not link `libm` | Link the math library explicitly, or avoid `c/math` |
| Online playground | Website build and editor tests | Parity with the current compiler and stdlib pack | The playground is not a verified beta try-it path | Use the local `zithc check`/`run` path |

## The supported first-use path

On Linux x86-64 with the GNU runtime:

1. Install `zithc` with the fetch installer or a source build.
2. Create a project with `zithc create hello-zith` and open it.
3. Run `zithc check` and correct any diagnostic it reports.
4. Run the project with `zithc run`.

See [First Program](doc:getting-started-quick-start) for the walkthrough and
[Implementation Status](neutral:reference-implementation-status) for the
per-feature compiler contract. The [Zith-- reference](doc:zith-subset-reference)
is the language contract, the [Standard Library](doc:reference-20-standard-library)
page lists the shipped modules, and [Editor Setup](doc:getting-started-editor-setup)
covers the VS Code path.
