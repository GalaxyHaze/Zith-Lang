# Self-Zith

This repository designs a self-hosting Zith compiler written in `Zith--`. Its
implementation is independent of the current `../Zith` toolchain; this glossary
is the shared vocabulary for the design. It is a glossary only, not a spec.

## Language

**Self-Zith**:
The self-hosting compiler this repository designs and will implement in `Zith--`.
_Avoid_: "the new compiler", "Zith 2".

**Zith--**:
The language subset the current `../Zith` `main` toolchain compiles; also the
language Self-Zith is written in.
_Avoid_: "the subset".

**HIR**:
High-level IR, the last front-side representation, formed after semantic
analysis and, here, after the ZPK proof phase.
_Avoid_: "middle IR".

**VIR**:
VM v2 IR, the typed execution IR between HIR and the VM/tiny backend. VIR is not
LLVM IR.
_Avoid_: "the VM IR" (VIR is the canonical name).

**Backend**:
A consumer of HIR that produces executable output. The VM (through VIR), the
reused LLVM emitters, and the future tiny backend are backends.
_Avoid_: "target", "codegen" (codegen is the act; a backend is the module).

**ZPK**:
Zith Proof Kernel, the coordinated proof phase (NIA, RRA, MRA, NRA) that runs
after Sema and before HIR.
_Avoid_: "the proof analysis", "the proofs".

**NIA**:
Numeric facts: ranges, versions, headers, control-flow joins.
_Avoid_: "NTA" (old name).

**RRA**:
Geometry of slices, arrays, and memory regions.
_Avoid_: "region analysis".

**MRA**:
Static memory regions, heaps, pools, permissions, and typed streams.
_Avoid_: "memory analysis".

**NRA**:
Ownership, lifetime, and borrow decisions.
_Avoid_: "borrow checker"; "SRA" (SRA is the small Zith-- slice, not this).

**External seam**:
The single narrow public interface of the compiler core
(`compile(input) -> result`); CLI, filesystem, process control, and linking stay
outside it.
_Avoid_: "public API", "facade".

**Stage**:
A responsibility within the pipeline (Lex, Scan, Import, Resolve, Sema, ZPK,
HIR, VIR). A contract, not necessarily a module; a module may own several.
_Avoid_: "pass", "phase" (phase is reserved for the distinct ZPK phase).

**Driver**:
A consumer of the external seam that lives outside the core: the thin CLI
driver, or the language-server driver.
_Avoid_: "frontend", "entry point".

**Workspace**:
The language-server driver's persistent context, owned across many document
snapshots. The core holds no such state.
_Avoid_: "session" (the core is not a session).

**Snapshot**:
One immutable revision of a document, handed to the core as its input.
_Avoid_: "version", "buffer".

**Residual facts**:
Proof results the ZPK phase emits into side tables (ownership, bounds,
region-validated access), stored in the cache so callers do not reanalyze
callee bodies.
_Avoid_: "proof state".
