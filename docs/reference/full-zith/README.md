# Full-Zith Reference (Imported)

Design material for the full Zith language, imported verbatim from the sibling
`Zith` repository (`GalaxyHaze/Zith`) so the self-hosting effort can consult it
from this checkout.

**These are design references, not decisions of this repository.** Nothing here
is implemented, required, or committed to by `self-Zith`. This repository
implements the compiler in `Zith--`; when a full-Zith idea is adopted here it
must become a local decision (a local ADR or `CONTEXT.md` term) with its own
record, and this imported copy stays a reference only.

## Provenance

- Source repository: `https://github.com/GalaxyHaze/Zith`.
- Base commit: `5a4177fc0f36a2dc06caa9b820668e86fe9afb86` (`main`).
- Import date: 2026-10-14.
- Each file is a verbatim copy except for the prepended provenance banner. The
  original numbering and file names are preserved.

Content state per file (the source checkout had uncommitted work at import):

- **Tracked at `5a4177fc`**: copied as committed.
- **Working tree, modified**: copied from the working tree; differs from
  `5a4177fc` by uncommitted edits present at import.
- **Working tree, untracked**: not committed upstream; exists only in the
  working tree at import.

## ADRs

All are accepted full-Zith design directions unless noted. Original numbering
is kept.

| File | Topic | State |
|---|---|---|
| `adr/0015-full-zith-thread-fork-merge.md` | `fork`/`merge`/`revoke` core keywords | tracked; superseded in part by 0035 |
| `adr/0026-nra-aggregate-nodes-and-revokable-flows.md` | AggregateNode, revocable flows, proxy | tracked; `.acquire()` superseded by 0041 |
| `adr/0029-contextual-syntax-continuations.md` | forward syntax continuations for words/contexts | tracked |
| `adr/0030-failable-invalid-state-model.md` | `Nil`, `Failable`, `try`/`or`/`!`/`fail`/`catch` | tracked; spec-only |
| `adr/0031-static-interfaces-and-trait-composition.md` | structural interfaces, trait composition | tracked |
| `adr/0032-universal-api-project-identity-and-contexts.md` | universal API style, project identity | tracked |
| `adr/0033-nra-reference-model-and-bind.md` | `&` / `^` bind / `%` own reference model | tracked |
| `adr/0034-nra-surface-spellings.md` | sigil/keyword equivalences, revocable `'`/`grant` | tracked |
| `adr/0035-nra-thread-separation-supersedes-forkcount.md` | removes `forkCount` and `MultiShare<T>` | tracked |
| `adr/0038-call-site-reference-annotations.md` | call-site transfer-mode sigils | tracked |
| `adr/0039-full-zith-capability-and-purity-contracts.md` | `@using`/`@only`, purity classes | working tree, untracked |
| `adr/0040-full-zith-surface-names-and-keyword-limit.md` | `contract`/`pod`/`extend`, 8-byte keyword limit | working tree, untracked |
| `adr/0041-revocable-guard-hold-spelling.md` | guard uses `.hold()` not `.acquire()` | working tree, untracked |

## Specs

| File | Topic | State |
|---|---|---|
| `specs/Zith-spec.md` | full Zith language spec draft v0.9 | working tree, modified |
| `specs/proof-model.md` | Zith Proof Kernel (ZPK): NIA/RRA/MRA/NRA | tracked |
| `specs/nia-spec.md` | Numeric Interval Analysis | tracked |
| `specs/rra-spec.md` | Region Relationship Analysis | tracked |
| `specs/mra-spec.md` | Memory Region Analysis | tracked |
| `specs/nra-spec.md` | Node Resource Analysis (source of truth for NRA) | working tree, modified |

## Plans

| File | Topic | State |
|---|---|---|
| `plans/branch-protocol.md` | full-Zith thread protocol (`fork`/`merge`/`revoke`) | tracked |
| `plans/callable-thread-blueprints.md` | job blueprints, closures, `Functor` | tracked |

## Notes

| File | Topic | State |
|---|---|---|
| `notes/nra-design.md` | operational pointer to the NRA contract | working tree, modified |
| `notes/branch-protocol-draft.md` | summary of the branch protocol draft | tracked |
| `notes/comptime-generics-traits.md` | 0.7.0 comptime/generics/traits boundaries | tracked |

## Known Contradictions

- `notes/branch-protocol-draft.md` still states that "`forkCount` remains the
  core share-node rule". This contradicts accepted ADR-0035, which removes
  `forkCount` and `MultiShare<T>`. Treat the note as stale on that point;
  ADR-0035 governs. `plans/branch-protocol.md` already carries the correct
  supersession banner.

## Broken Relative Links

The imported files are verbatim, so their relative Markdown links still point
into the `../Zith` tree (for example `../nra-spec.md`, `adr/0034-*.md`, and the
chapter links in `specs/Zith-spec.md`). Those targets do not exist here. This is
expected for a verbatim import; resolve them against `../Zith` if you need to
follow one. The links are intentionally not rewritten, to keep the copies
byte-identical to their source.

## Deliberately Not Imported

- `CONTEXT.md` and the chapter docs (`docs/NN-*.md`) of `../Zith`: the glossary
  and chapters there describe the current `Zith--` compiler, not full-Zith
  design, and would collide with this repository's own `CONTEXT.md` once one
  exists.
- `docs/Zith--.md` and `docs/impl-status.md`: they describe the existing
  `Zith--` toolchain's implemented behavior, which is a separate concern from
  the full-Zith design and from this repository's new implementation.

Consult `../Zith` directly for those; this folder is only the full-Zith design
set.
