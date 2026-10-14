# Stages are contracts over immutable outputs with a shared context

Each stage is `stage(immutable input, shared context) -> immutable output`. The
shared context carries the arenas, the string/type interner, and the diagnostic
collector, and is passed by reference because it is expensive to copy and
genuinely common to every stage. Stages report failure through an explicit
`Result`; a stage's errors block the following stages while warnings do not.

Immutable outputs make each stage testable in isolation: a test feeds a
hand-built input and reads the output across the same seam a caller uses. A
single mutable context that stages enrich was rejected: that is what made the
previous `CompilationSession` drag options, paths, linking, and execution along
with it. Drivers do not stitch stage state together by hand.
