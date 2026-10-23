# Domain Docs

This repository is the full Zith language and compiler project. `Zith--` and C
are bootstrap implementation substrates, not the project's destination:
project-owned code is expected to migrate incrementally to full-Zith syntax and
Zith implementations as the compiler gains the required capabilities.

## Before exploring

Read the root `CONTEXT.md`, or `CONTEXT-MAP.md` if one exists, and the relevant ADRs in `docs/adr/`. If these files do not exist, proceed silently; create them lazily when domain terms or decisions are established.

## Zith references

The sibling `../Zith` checkout contains the current bootstrap toolchain and
related design material:
- `docs/Zith--.md` is the compiler-subset contract.
- `docs/Zith-spec.md` describes the broader full-Zith design.
- `docs/impl-status.md` records what the current toolchain implements; it is not
  the full-Zith implementation status for this repository.

When consulting those documents, distinguish implemented bootstrap behavior
from full-Zith design. The full-Zith direction is in scope; individual imported
proposals still need reconciliation before they become canonical local
contracts.

Full-Zith design material (ADRs, ZPK specs, branch-protocol and NRA notes) is
imported verbatim under `docs/reference/full-zith/`, with provenance in its
`README.md`. Treat it as the design baseline to reconcile, not as an
automatically consistent local specification. Preserve the imported copies;
record reconciled local decisions in the local specification or ADRs.

Use the terminology established in this repository's `CONTEXT.md`. Surface
conflicts between imported material, local ADRs and implementation status
instead of silently overriding them. Follow `docs/plans/roadmap.md` for the
bootstrap and migration sequence. Migrate syntax or C implementation one
verified slice at a time, keeping a working bootstrap path until the next
compiler generation can build and test that slice.
