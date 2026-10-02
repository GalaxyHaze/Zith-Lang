# Forward Syntax Continuations for Contextual Words

Status: accepted

## Context

SQL-like DSLs need to express clauses in source order without treating every
clause word as an ordinary value-returning operator. A following clause must
not reach backward and reinterpret already parsed syntax. Contextual infix
chains also need a consistent ambiguity rule rather than an implicit choice
of associativity.

## Decision

A contextual syntax operation produces either an ordinary language value or
a syntax continuation. A continuation is parse-time syntax state, not a
runtime value. It consumes following syntax in source order and resolves to an
ordinary value or another continuation.

For example, in `SELECT a FROM b`, `SELECT a` produces a continuation that
consumes the following `FROM` clause and its source operand `b`. This lets the
surface syntax follow SQL clause order without making `FROM` associate
backward over the completed `SELECT a`.

The parser rejects an expression whenever its syntax admits multiple valid
trees. This includes ambiguous chains of mixed or repeated contextual infix
operators, such as `a DOT b CROSS c` and `a DOT b DOT c`. Parentheses express
the intended grouping, as in `(a DOT b) CROSS c` or `a DOT (b CROSS c)`.
The parser does not choose an implicit associativity to resolve such cases.

This decision records the intended language model only. It does not change
the implementation status of words or contexts, which remains parse-skipped.
The declaration syntax and the relationship between `nop` and existing
`token` words remain outside this decision.

## Consequences

- Contextual syntax can express ordered, multi-clause DSL forms with
  forward-only continuations.
- Each continuation must identify the following syntax it accepts and whether
  it resolves to a value or another continuation.
- Ambiguous operator chains require explicit grouping instead of depending on
  an implicit left-to-right or right-to-left convention.
- Continuations do not introduce runtime values or runtime builder objects.
