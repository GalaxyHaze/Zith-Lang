<!--
Imported reference. Source: GalaxyHaze/Zith@5a4177fc:docs/adr/0040-full-zith-surface-names-and-keyword-limit.md (working tree, untracked).
Design reference only; not a decision of this repository. See ../README.md.
-->

# Full-Zith Surface Names and Keyword Limit

Status: accepted as a full-Zith design direction. It does not change Zith--.

Full Zith uses `contract` for the structural static declaration previously
called `interface`, and `pod` for the POD declaration previously called
`component`. Nominal trait conformance uses `extend Type with Trait { ... }`.
Trait composition remains `trait Child extends Parent`. The semantic contract
of structural declarations remains the one recorded in
[ADR-0031](0031-static-interfaces-and-trait-composition.md). This decision
supersedes its `interface` declaration spelling, not its structural semantics.

Every full-Zith keyword spelling is limited to eight ASCII bytes. The lexer
may pack each spelling into a `u64` lookup key, preserving the exact spelling.
An audit of the current Zith-- lexer found three spellings over this limit:
`component`, `interface`, and `implement`. The full-Zith spellings above replace
them with `pod`, `contract`, and `extend`, respectively. Zith-- retains its
existing spellings.

The limit is a full-Zith language-surface constraint motivated by lexer
implementation, not a source compatibility rule for Zith--. Whether the
related `@component` type-kind predicate is renamed to `@pod` remains open.
The spelling for an inherent method block without a trait is also not fixed by
this decision.
