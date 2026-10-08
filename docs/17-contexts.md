## 17. Contexts

> **Status:** see [impl-status.md](impl-status.md) for the current status of contexts.
> `context` declarations parse but their bodies are dropped.

> **Full-Zith design direction:** contexts are reserved for syntax integration
> with a domain-facing API. A context is not an ordinary public API surface or
> a general-purpose container for declarations. See
> [ADR 0032](adr/0032-universal-api-project-identity-and-contexts.md).

A context provides an optional syntax integration for a domain such as Math,
SQL, or HTML. A domain API may offer one when it deliberately participates in
that domain's syntax. The current draft allows scoped or global activation,
with only one context active at a time in a scope.

```zith
// Illustrative syntax: SQL-specific forms are active inside this block
use SQL {
    SELECT * FROM users WHERE id = :id
}

// Global activation is also present in the current draft
use SQL;
```

### Best Practice

Use a context when an API deliberately integrates with domain-specific syntax.
Do not use one merely to group ordinary public declarations or to create a
generic library namespace. Exact declaration, activation, and distribution
rules remain open design questions.

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
