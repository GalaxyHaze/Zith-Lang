# The compiler core exposes one narrow external seam

The Self-Zith core presents a single narrow interface, `compile(input) ->
result`, where the result carries diagnostics and artifacts. The command-line
interface, filesystem access, process control, and native linking live outside
the core, in drivers.

One interface gives leverage to the CLI driver, the tests, and a future
bootstrap driver, and gives locality: changing a stage does not ripple out to
callers. Exposing a public interface per stage was rejected: it would reify the
stage list as structure and leak the pipeline interior to every consumer.
