# Sequenta Modules (Archived)

Summary of the Sequenta modules executed in this repository and later removed
from the working tree. All modules reached status `done`. The full module and
context files remain available in git history.

## sequenta-attributes (done 2026-09-23)

End-to-end support for `@`-attributes in the Zith-- frontend: lexing `@`,
attribute AST nodes, parsing top-level and statement attributes, symbol
propagation, target classification, `@discardable` through sema/HIR and
persistence, `@volatile` through NRA and codegen, formatter support, tests,
and docs.

15 tasks: lexHash, addAttributeAst, parseTopLevelAttributes,
parseStatementAttributes, propagateSymbols, classifyAttributeTargets,
semaDiscardable, hirDiscardable, persistDiscardable, nraVolatile,
codegenVolatile, formatAttributes, addTests, updateDocs, finalVerification.

## seq-vm (done 2026-09-19)

Typed, linear VM v2 prototype built as a harness separate from execution IR
v1 (`src/vm/`, contract VMV2-01). The `v2-host` module provided typed-ir,
vm-v2, linear-memory, and runtime-ffi symbols; the `v2-slice` module built
the VM shape, function references, linear memory ops, FFI calls, and
hello-world stdlib/extern programs, closing with an integration task
(`ctest -R test-vm-v2`).

The implementation orientation note survives at
`archive/seq-vm-orientation.md`.

## seq-docs (done 2026-09-29)

`zithc docs` command pipeline: CLI contract, project graph and reachable
module set, documented symbol/API model, diagnostics policy, deterministic
markdown rendering, safe output file policy, tests, and published CLI
documentation.

8 tasks: cliContract, projectGraph, symbolModel, errorPolicy,
markdownRenderer, outputPolicy, tests, userDocs.

## seq-emit (done)

Deterministic textual dumps for the playground and CLI: wasm-safe text
output sink, CST printer, VM v2 (VIR) printer, cumulative `--dump` CLI
flags, textual WASM ABI output, tests, and published docs.

7 tasks: outputSink, cstPrinter, virPrinter, cliFlags, wasmAbi, tests,
docs.
