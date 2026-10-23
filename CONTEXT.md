# Zith

This repository is the home of the Zith language and compiler project. This
file records shared vocabulary only; language rules belong in the reconciled
Zith specification, and implementation decisions belong in ADRs.

## Language

**Zith**:
The language and compiler project whose target is the full Zith design.
_Avoid_: "the new compiler", "Zith 2".

**Zith--**:
A sibling language to Zith with its own frontend and semantic rules. It is
also used as a bootstrap implementation substrate; it is not merely a subset
of Zith.
_Avoid_: "legacy subset".

**Bootstrap compiler**:
A compiler generation used to build and verify the next compiler generation.
_Avoid_: "the final compiler" when referring to an intermediate generation.

**HIR**:
High-level IR, the shared representation produced by distinct language
frontends for constructs whose semantics are compatible, after semantic
analysis and, here, after the ZPK proof phase.
_Avoid_: "middle IR".

**VIR**:
VM v2 IR, the typed execution IR derived from HIR for the VM/tiny backend. VIR
is not LLVM IR; reuse requires compatible VM semantics.
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
Proof results the ZPK phase emits alongside HIR (ownership, bounds, and
region-validated access). Their absence never implies safety; a consumer must
retain the required runtime check or recompute the proof.
_Avoid_: "proof state".

**HIR compatibility**:
The claim that two frontends represent a construct with the same semantics in
the shared HIR contract. Source syntax or matching source text alone does not
establish compatibility.
_Avoid_: "same-source compatibility".
