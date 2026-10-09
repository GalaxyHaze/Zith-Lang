# NRA Thread Separation Supersedes forkCount

Status: accepted as a full-Zith design direction. Nothing in this decision is
implemented or changes Zith--.

## Context

ADR-0015 and `docs/plans/branch-protocol.md` describe threads with a
`forkCount` ownership counter on `share` nodes, and the earlier NRA spec had a
`MultiShare<T>` transport wrapper that carried that counter. Both make the
analysis carry per-node dynamic state across flow creation.

The NRA rewrite proves each thread as a separate program over its own
resource graph. That removes the need for a shared dynamic counter: the only
obligations at a flow boundary are the two construction rules in NRA-10.

## Decision

NRA proves each flow separately and does not track how many flows hold a
resource. There is no `forkCount` counter and no `MultiShare<T>` transport
type in the NRA core model.

Composition safety comes from construction instead:

- A bounded flow is merged before the end of the creating scope. The compiler
  checks this structurally.
- An unbounded flow has its access revoked at the end of the creating scope,
  through the revocable proxy of ADR-0026.

`fork`, `merge`, and `revoke` remain core keywords and the handle lifecycle
from ADR-0015 is unchanged. Only the `forkCount` accounting and the
`MultiShare<T>` wrapper are removed. A live unmerged handle at scope exit
stays an ownership error, now expressed as a frontier rule rather than a
counter.

## Consequences

- ADR-0015's statement that "NRA keeps `forkCount` as the ownership rule for
  `share` payloads" is superseded.
- `docs/plans/branch-protocol.md` keeps the `fork`/`merge`/`revoke` surface
  but its `forkCount` sections are superseded.
- Cross-flow writes are gated by capability, not by a counter: NRA-11.
- The spec sections 2.5 and 7 are the current contract.

## Considered Options

Keeping `forkCount` and proving the counter alongside the per-flow proof was
rejected: it duplicates state that the per-flow proof already implies, and it
forces every node that might cross a flow to carry transport metadata.
