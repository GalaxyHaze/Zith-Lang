# Domain Docs

This repository is the self-hosting effort: the compiler will be written in `Zith--`. Keep implementation decisions here separate from the broader full-Zith design.

## Before exploring

Read the root `CONTEXT.md`, or `CONTEXT-MAP.md` if one exists, and the relevant ADRs in `docs/adr/`. If these files do not exist, proceed silently; create them lazily when domain terms or decisions are established.

## Zith references

The sibling `../Zith` checkout is the current design/reference source:
- `docs/Zith--.md` is the compiler-subset contract.
- `docs/Zith-spec.md` describes the broader full-Zith design; it is design input, not an implementation requirement here.
- `docs/impl-status.md` records implementation status for that compiler.

When consulting those documents, distinguish implemented behavior from spec-only design. Do not bring a full-Zith feature into this self-hosting scope unless we explicitly decide to. Record the source and status when carrying a design note into this repository.

The full-Zith design material (ADRs, ZPK specs, branch-protocol and NRA notes)
is also imported verbatim under `docs/reference/full-zith/`, with provenance in
its `README.md`. Those files are design references, not decisions of this
repository; an adopted idea still needs a local ADR or `CONTEXT.md` term.

Use the terminology established in this repository's `CONTEXT.md`. Surface conflicts with its ADRs instead of silently overriding them.
