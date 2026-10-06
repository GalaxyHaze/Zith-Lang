# Universal API Style, Project Identity, and Domain Contexts

Status: accepted as a full-Zith design direction. Project feature-policy
details and the remaining context and tag declaration rules remain open.
Nothing in this decision is implemented or changes Zith--.

## Context

Zith supports different implementation styles. Those styles should be able to
share familiar public APIs without forcing every implementation to use the
same style. Teams also need a way to keep their own codebases consistent
without imposing local choices on dependencies.

Contexts are useful for domain-specific syntax, but treating them as generic
API containers would make them an ordinary library abstraction rather than a
specialized language feature.

## Decision

Zith defines a universal public-API style independently of project identities.
The style is intended to be recognizable to users of Zith regardless of how
an implementation is written. For example, three implementations of
`classify(score: i32): i32` may use different styles while exposing the same
callable API.

The universal style favors:

- Returning a tuple when an operation naturally produces multiple values,
  rather than using output parameters.
- Not returning compile-time `type` values, raw function pointers, or `dyn fn`
  directly from ordinary APIs. An API should manage the mechanism or expose a
  callable object with a meaningful contract when callers need one.
- Not exposing `dyn` dispatch in the universal style. Traits and interfaces
  appear only in generic bounds.
- Keeping `@ensure`, `maybe`, and `assume` as implementation details unless
  they express a caller-relevant part of a specific API contract.
- `camelCase` method names.

These are API design conventions, not language restrictions. A public API may
depart from them when its domain or contract provides a clear reason.

A project identity is a separate, project-local policy for the project's own
codebase. It lets a team enable or disable optional language features and
attach an explanatory reason to a disabled feature. Those feature choices and
optional reasons are the team's controls in this policy. The compiler's
diagnostic remains consistent, while the project reason explains the local
rationale. The identity does not redefine Zith's universal API style, change
language semantics, or impose its feature choices on dependencies.

A context is not an ordinary public API surface. It is a specialized syntax
integration mechanism that a domain-facing API may offer when it participates
in a domain such as Math, SQL, or HTML. It must not serve as a general-purpose
public API container or a namespace for grouping ordinary declarations.

Full-Zith tags are distinct from macros and compiler intrinsics. A tag
invocation uses `@<Tag> ... </Tag>`. The opening `@<` and closing `</` are
dedicated delimiters. The opening delimiter is not an `@` intrinsic followed
by the less-than operator. Outside this compound delimiter, `@` continues to
mark compiler intrinsics or compiler magic.

Every tag receives a required body argument, and its declaration specifies
how that body is represented. The established body kinds are:

- `tokens`: the exact body text as written, without interpreting it as Zith.
- `identifier`: exactly one identifier.
- `ast`: syntax already structured for inspection or transformation, but not
  necessarily evaluated.
- `block`: code parsed as a Zith block.

These body kinds are not an exhaustive list. Additional representations may be
defined when a domain integration needs them. The exact tag declaration
grammar and empty-body rules remain open.

## Consequences

- Different implementation styles can share one public API.
- Project-local feature policies do not fragment the language into dialects
  and do not alter dependency policies.
- Contexts remain a specialized domain-integration mechanism rather than a
  routine API abstraction.
- Full-Zith tag calls have an explicit delimiter and a declared body
  representation, so domain syntax does not have to masquerade as an
  intrinsic or a macro.
- The universal API style is guidance. It does not prohibit advanced language
  features in a public API when the API has a domain-specific reason.
- These decisions describe full Zith only. They do not change the Zith--
  contract or implementation.

## Open Questions

- What configuration syntax names a project identity, its feature choices, and
  optional diagnostic reasons?
- How should a disabled-feature reason appear alongside the compiler's
  standard diagnostic?
- Which feature choices belong to a project identity, and which affect
  semantics or public contracts too deeply to be local policy?
- What exact declaration and attribute syntax do tags use?
- Can a tag receive an empty body, and how is that represented?
- What exact activation and distribution rules apply when a library offers a
  domain context?
