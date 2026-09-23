# ZithProofKernel: Unified Proof Model Draft

Status: draft for discussion, not a ratified specification.

This document unifies the publicly documented ZPK model with the proposal for
a fifth subsystem focused on representing proven facts as backend decisions.
It intentionally separates what is already stated upstream from what is a new
claim in this repository.

## 1. Purpose

The Zith Proof Kernel (ZPK) is the coordinated proof and safety core of Zith.
The public-language docs currently expose the ownership-oriented parts of this
system, primarily NRA. The repository-level docs also describe NIA, RRA, MRA,
and NRA as the four ZPK sub-systems.

The goal of this draft is to define a single authoritative mental model:

- State the stable public contract: ZPK coordinates proof facts across
  analyzers, and NRA remains the ownership-facing layer.
- Make the proposed fifth subsystem explicit: TRO is the representation layer
  that decides how already-proven facts are manifested in the backend.
- Record open questions instead of pretending the language already has a full
  proof backend.

## 2. Existing Four Subsystems

The upstream proof model names four cooperating subsystems.

| Name | Domain | Consumed by |
|---|---|---|
| NIA | Numeric intervals, ranges, versions, control-flow joins. | RRA, MRA, HIR/codegen. |
| RRA | Geometry of slices, arrays, and memory regions. | NRA, MRA, HIR/codegen. |
| MRA | Static memory regions, heaps, pools, permissions, streams. | RRA, NRA, codegen. |
| NRA | Ownership, lifetime, and borrow decisions. | NIA, RRA, HIR, cache. |

The documented pipeline order is:

```text
source -> lex -> scan -> import -> resolve -> sema -> ZPK -> HIR -> codegen -> cache
```

Inside ZPK, the intended dependency order is:

```text
NIA -> RRA -> MRA -> NRA
```

That order is not a single monolithic pass. It is a contract: numeric facts
precede geometry, geometry precedes ownership, and ownership concludes the
proof before a stable HIR boundary is emitted.

## 3. Proposed Fifth Subsystem

The proposed fifth subsystem is TRO (Transformation and Optimization), but its
role is narrower than a classic optimizer. TRO does not discover new proof
facts. It decides how already-proven facts are represented in the backend.

The proposed boundary:

| Subsystem | Owns | Does not own |
|---|---|---|
| TRO | Representation decisions, transformation decisions, and backend-oriented residuals. | Fact discovery, ownership, memory permissions, runtime allocation policy. |

TRO consumes facts from NIA, RRA, MRA, and NRA. If a representation would
require a fact that those subsystems did not prove, TRO must not invent it.

## 4. TRO On Effect Headers

TRO operates primarily on effect headers, not on arbitrary function bodies.
An effect header records what a function reads, writes, moves, borrows, and
returns. TRO may use those headers to choose a representation.

Example from the discussion:

```text
effect-header:
  returnsArgument(0)
  moves arg 0
```

If a function always returns the same node that it moved from argument 0, TRO
can manifest the call as a reference-based pass without creating a new return
value. The resource already exists and was moved; the new representation does
not need to manufacture another owned result.

This is a representation decision enabled by NRA facts. It is not a generic
claim that returning by reference is always safe or always better.

### 4.1 Signature Vs Internal Decision

Changing how a function is represented may change its signature or ABI. That
is not forbidden, but it has a consequence: every caller and every cached
effect-header must agree on the new representation.

The rule for this draft is:

- TRO may change internal representation when the change is local and fully
  visible in the same compilation unit.
- TRO may change a public signature only when the effect header, cache entry,
  and all call sites are updated together.
- TRO must not silently keep a stale representation in a compiled library.

## 5. What "Not Heavy" Means

The phrase "the value is never heavy" is about the backend, not about runtime
cost measurement. A tiny backend is simple, compact, and relatively slow by
design. It is not a production optimizer.

Therefore TRO-solo should be defined as:

- use facts that already exist;
- emit simple, correct, small code;
- do not attempt broad optimization passes;
- prefer removing work that the facts make redundant;
- degrade to a correct conservative form when no fact is available.

TRO does not need a sophisticated cost model. It needs a clear rule for when a
fact is sufficient for a representation decision.

## 6. Two Backend Modes

The draft separates two modes because their guarantees differ.

### 6.1 TRO-LLVM

Zith normally lowers through LLVM. In this mode TRO:

- converts proven facts into backend hints;
- keeps LLVM as the optimization authority;
- does not claim that TRO proves the final optimization;
- may emit attributes such as `noalias`, `readonly`, `nocapture`, or
  `unchecked_bounds` when the corresponding fact exists.

LLVM is not the source of truth for ownership. It receives hints only after
NIA/RRA/MRA/NRA have proven the relevant facts.

### 6.2 TRO-Solo

When Zith runs without LLVM, TRO becomes the backend optimizer. In this mode
TRO:

- owns the optimization decisions that LLVM would otherwise make;
- remains limited by the facts available in the proof layer;
- does not become a new safety analyzer;
- keeps the same fact layer shared with TRO-LLVM.

TRO-Solo and TRO-LLVM should share the same fact-consumption core and differ
only in policy. This avoids duplicating the analysis.

## 7. Representation Contracts

To keep TRO testable, each representation decision should have a contract:

| Decision | Required fact | Example |
|---|---|---|
| Omit return copy | `returnsArgument(i)` + move | Return the moved node by reference. |
| Remove bounds check | NIA `0 <= i < len` | Emit unchecked access. |
| Keep region access safe | MRA permission + RRA containment | Emit raw access without runtime region table. |
| Remove dead store | NRA `dead(value)` | Skip an unobservable store. |
| Preserve read-only call | effect header `read`/`view` | Emit `readonly` or `nocapture`. |

The contract must record which proof facts were used. If the facts change,
the decision must be invalidated.

## 8. Witness And Versioning

TRO should keep a lightweight witness record:

```text
representation decision
input effect-header version
facts used
output representation
analysis versions
```

The witness is not a full theorem prover output. It is enough to say "this
representation choice used these proven facts." It must be tied to the
analysis versions that produced those facts.

Without versioning, a cached representation may survive a change to NIA, RRA,
MRA, or NRA and silently remain valid after the proof changed.

## 9. Boundaries And Rules

- TRO does not create safety facts.
- TRO does not re-prove ownership from LLVM or from backend IR.
- TRO may choose a conservative representation when facts are missing.
- Contradiction is an error, not a fallback.
- `Unknown` remains conservative.
- TRO witnesses must be checked against cached headers, not against uncached
  full-function bodies.

## 10. Naming

TRO is a placeholder. Candidate names must be checked for collision with
existing Zith concepts.

| Name | Risk |
|---|---|
| TRO | Novel but needs definition. Shared root with existing transforms. |
| POM | Proof/optimization module; may collide with existing tooling names. |
| OPF | Optimization proof facts; too abstract for a module. |
| NRA-OPT | Bad: NRA owns ownership; optimization is not a flavor of ownership. |
| ZPK-Opt | Clear umbrella name, but invites treating optimization as outside the proof kernel. |

Until a name is chosen, this draft uses TRO and marks it as tentative.

## 11. Cross-Cutting Concerns

### 11.1 Effect Headers And Cache

TRO depends on function effect headers. The current NRA spec already requires
call-site analysis without re-analyzing callee bodies. If TRO needs caller-side
facts that are not in those headers, the cache contract must be extended in
the same document.

### 11.2 Stable HIR Boundary

The stable HIR boundary must not be changed by TRO. TRO facts may attach to
HIR as residual side data, exactly like NRA residual facts. They cannot be the
place where ownership is decided.

### 11.3 LLVM Boundary

LLVM already performs optimizations. ZPK-TRO should not compete with LLVM at
the IR level. It should either prove facts that LLVM can translate into safe
hints, or prove facts that the Zith frontend needs before LLVM sees the
program.

## 12. Repository Layout

This repository should hold a draft that unifies documentation, but it should
not duplicate the upstream specs byte-for-byte. The draft should be:

- a summary of the current four-subsystem model;
- the rationale for TRO;
- the open questions that block implementation;
- a pointer to the authoritative upstream files;
- a changelog showing what changed in this idea.

Proposed directory layout:

```text
docs/
  zpk/
    README.md
    proof-model.md
    nia.md
    rra.md
    mra.md
    nra.md
    optimization.md
```

The `optimization.md` file should start as an open proposal, not as a
completed specification.

## 13. Open Questions

1. What is the observability specification for Zith? TRO needs it to say when
   a representation is behavior-preserving.
2. Does TRO need full proof artifacts for every decision, or only for
   user-requested proof modes?
3. Is TRO a pass, a fact layer, or a representation backend? This draft
   assumes a representation backend that consumes shared facts.
4. How do representation witnesses survive cache invalidation and
   cross-module builds?
5. Which transformations are in scope for TRO-Solo version one? Proposed
   first scope: header-driven return representation, bounds-check removal,
   safe dead-store removal, and allocator-local reuse.
6. How do representation failures interact with diagnostics? A failed
   decision must not silently degrade to unsafe code.
7. Should TRO use `@assume`, `@ensure`, and `@maybe` contracts like MRA, or
   use a separate representation syntax?

## 14. Summary

Current status: the four-subsystem model is documented upstream. TRO is a
plausible fifth subsystem when defined as the representation layer over
proven facts.

The strongest framing is: TRO does not optimize by discovering facts. It
optimizes by deciding how facts already discovered by NIA/RRA/MRA/NRA are
represented, using effect headers as the trust boundary.

## 15. Thread Model Pointer

Threads are the most likely area to change the unified model. The round-level
draft in [draft-thread-model.md](draft-thread-model.md) records the decisions
made so far and the questions still blocking a stable contract.
