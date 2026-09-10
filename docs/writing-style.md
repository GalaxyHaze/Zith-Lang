# Writing Style

Three genres cover the writing the project produces: spec, docs, and log.
Each has its own shape and audience. A few habits apply everywhere: write in
plain prose short enough to scan, keep the glossary terms from `CONTEXT.md`
when a domain name is involved, and read the shared rules below before
writing or reviewing.

## Shared rules

- No emojis, anywhere.
- No em dashes. A hyphen stays inside a compound word; a comma, colon, or
  restart is better than an em dash.
- No semicolon used as a period. A semicolon is valid only when two clauses
  genuinely share one idea; otherwise use a comma or split the sentence.
- Write in the active voice and keep sentences to one idea when possible.

### Before and after

Em dash:

> The compiler uses an arena-backed store. It never frees during a pass, and
> the pass owns it.

Not:

> The compiler uses an arena-backed store, and the pass owns it — that's why
> it never frees during the pass.

Semicolon:

> Resolution finished. The parser fed it one module at a time.

Not:

> Resolution finished; the parser fed it one module at a time.

## Spec

Specs are the directive contract for what must be true. They answer
"what does the system guarantee?". They use precise, testable language and
keep the subject explicit.

Structure: a short status line (draft, accepted, implemented), a one-paragraph
purpose, then the requirements in the order a reader would verify them. Each
requirement starts with the actor or mechanism, not with "we" or "should".

> `expect` evaluates its argument once and must fail the enclosing function
> with the attached message when the value is false.

## Docs

Docs teach a reader how to use something. They answer "how do I do X?". They
lead with the task or decision a reader faces, not with implementation detail.

Structure: a short title, a one-line "what this covers", then the steps or
options with the concrete gain before the mechanics. Keep one use case per
section. This genre is the closest to the site at `../Zith-website`.

> Want a debug string for a type? `@printType(T)` returns it for any type
> with a known representation, and accepts an optional stride hint.

### Sample

The traits sample you shared reads like good docs already. It starts with the
mechanism, gives code, then explains the edge cases. The fields that need
cleanup are the semicolons as separators inside prose and the em dash before
"that's why". Those become commas or full stops.

> `Point` explicitly implements `Printable`, so `expectTrait` accepts it and
> rejects `Foo`. `expectInterface` only cares whether the type has a `print`
> method, so it accepts both.

> Use **Traits** for more control, and when context matters more than the
> method name. For example, `save()` is too generic. Where will it save, in a
> file, in JSON, or in a database? The method itself does not explain it.

> **Interfaces** fit better when the method name explains what it does, such
> as `area()` or `print()`.

## Log

Logs record what actually happened, in time. They answer "what did we do,
when, and why". They are the private record: terse, ordered, and honest about
open questions. They never promise work that is not started.

Structure: a date, a short headline, then the events in chronological order.
State the decision and the evidence, then leave open questions at the end.

> Entry: 2026-09-10, got a shared writing style
>
> For months the docs and logs were placeholders, and honestly, a lot of the
> earlier versions were AI slop. This entry marks the moment I started paying
> real attention to writing them. A lot happened in the last few months. I
> hope people like the work, and that I can finally say "we" and "us" instead
> of "Me" and "I".
>
> End of entry.

## Where each genre lives

Put specs in `docs/`, user-facing docs in the site repo, and logs in
`memory/` or the nearest dated file. If a file serves two genres, say so at
the top and separate the sections rather than mixing them.
