# Issue tracker: GitHub

Issues for this self-hosting work are tracked in `GalaxyHaze/Zith-Lang` GitHub Issues. Use `gh --repo GalaxyHaze/Zith-Lang` so operations do not depend on this checkout having a configured remote.

## Conventions

- Create, read, list, comment on, label, and close issues with the corresponding `gh issue` commands.
- When a skill says to publish work, create a GitHub issue. When it says to fetch a ticket, run `gh --repo GalaxyHaze/Zith-Lang issue view <number> --comments`.
- **PRs as a request surface: no.** Triage issues, not pull requests.

## Wayfinding

Use one issue labelled `wayfinder:map` as the map, with child issues as tickets. Link children as GitHub sub-issues when available. Use native issue dependencies for blockers; otherwise record `Blocked by: #<number>` in the child issue. Claim by assigning the issue to `@me`. Resolve by commenting with the answer, closing the issue, and adding a context pointer to the map.
