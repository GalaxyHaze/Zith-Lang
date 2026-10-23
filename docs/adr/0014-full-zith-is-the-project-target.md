# Full Zith is the project target

**Status: accepted (2026-10-14).**

This repository develops the full Zith language and compiler. `Zith--` and C
are bootstrap substrates: as the compiler gains the required capabilities,
project-owned source should migrate incrementally to full-Zith syntax and Zith
implementations. Preserve a working bootstrap and migrate only slices whose
behavior is covered by tests. Keep C at a platform or ABI boundary only while
that boundary is still required and explicitly documented; do not treat C as
the permanent implementation language by default.

This decision sets the destination for the compiler architecture in
ADRs 0001-0013; those decisions describe the initial compiler shape unless
revisited by a later ADR.
