# Zith Compiler

Zith is a statically typed C++23 compiler targeting a minimal systems-language subset called `Zith--`. The modern pipeline is the canonical implementation surface; legacy parser/sema artifacts are archived outside the active tree.

## Language

**Zith macro**:
A source-level `macro` declaration in a `.zith` module. It defines a reusable syntax-template expansion that runs during frontend lowering, before semantic analysis.
_Avoid_: C macro, comptime function, inline function

**Macro argument kind**:
The declared contract for a macro parameter: `val`, `identifier`, or `ast`.
_Avoid_: parameter meta-type, metatype, expr parameter, body parameter

**Macro value argument**:
An evaluated argument produced by normal call-site expression semantics, without explicit unevaluated syntax.
_Avoid_: expression parameter, value expression, evaluated expression

**Macro identifier argument**:
An argument bound from a single unqualified `Name` token. Qualified names, fields, and imported members are not identifier arguments.
_Avoid_: name parameter, identifier expression, simple name argument

**Macro AST argument**:
An unevaluated syntactic expression requested at the call site with `=expr`, where `expr` may include a block literal.
_Avoid_: pass-by-AST, quoted expression, raw expression, unevaluated expression

**Macro call attribute**:
A named call-site attribute written in the `|name: value|` position and made visible through the macro body as `attributes.name`.
_Avoid_: annotation, tag attribute, macro attribute list

**Module**:
A `.zith` source unit that owns a set of public and module-local symbols and can be imported by other units.
_Avoid_: File, script, package

**Import**:
Load another module and bind it under a namespace or explicit alias without injecting its symbols into the current scope.
_Avoid_: include

**From-import**:
Load another module and inject its public symbols directly into the current module scope.
_Avoid_: wildcard import, using namespace

**Export**:
Make a dependency visible to consumers of the current module, both as its full path namespace and as injected public symbols.
_Avoid_: re-export alias, pass-through

**Module alias**:
The local name bound by `import Path as Name` that consumers use as the base of qualified access.
_Avoid_: namespace, shorthand

**Qualified name**:
A dot-separated route from an import alias or full module path to a public symbol.
_Avoid_: path access, dotted name

**Public symbol**:
A declaration visible to importing modules, written with `pub`.
_Avoid_: exported symbol, visible symbol

**Module path**:
The slash-separated location of a module relative to a visible import root, written with `/`, not `.`.
_Avoid_: package path, namespace path

**C macro constant**:
A scalar object-like `#define` imported from a C header and exposed to Zith as an immutable foreign constant.
_Avoid_: macro, define constant

## Architecture

**Frontend context**:
Owns module loading, import graph construction, public symbol merging, and per-module name resolution for the modern pipeline.
_Avoid_: importer, resolver, module loader

**Module resolution**:
The per-module mapping from expression nodes and bindings to concrete declarations, imports, and foreign C header entries.
_Avoid_: symbol table, binding table

## Pattern Matching

**When case**:
A single alternative of a `when` expression. It starts with a parenthesized condition area, followed by an optional `~>` marker and a body; `~>` is deprecated and remains only as an explicit escape.
_Avoid_: case, arm, branch, when branch

**When pattern island**:
The first parenthesized segment of a when case, always interpreted against the `when` subject.
_Avoid_: pattern block, first guard, subject block

**When continuation island**:
Any parenthesized segment after a top-level `and`/`or`/`xor` in a when case. It behaves as an ordinary boolean condition, not as another subject pattern.
_Avoid_: secondary pattern, multi-pattern, following guard

**When case separator**:
The mandatory comma between when cases, omitted for the final case.
_Avoid_: arrow delimiter, comma terminator, case delimiter

## Ranges, `Contains` e `Iterator`

**Range**:
A literal expression `lo..hi` producing an interval between two raw bounds.
The AST/HIR keep `lo` and `hi` unchanged and store `openAtLo`/`openAtHi`
instead of adjusting the bounds.
_Avoid_: slice range, pattern shorthand, adjusted interval

**Range Bound**:
One of the two endpoints carried by a `Range`. A bound is closed by default;
`>` before `..` opens the lower bound and `<` after `..` opens the upper bound.
_Avoid_: offset bound, low+1 bound, high-1 bound

**Contains**:
The duck-typed protocol resolved by `value in rhs`: `contains(self, value): bool`.
It is independent of iteration and accepts any RHS type with that method,
including literal ranges.
_Avoid_: membership operator overload, range checking method, iterator method

**Iterator**:
The loop protocol resolved by `for (x in iterable)`: `next(self): ?T`.
It is deliberately separate from `Contains`; literal integer ranges are lowered
directly with an implicit step of `1` and float ranges are rejected for loops.
_Avoid_: Contains, membership protocol, range iteration via contains

## Runtime And Backends

**Interpreter HIR**:
The in-process executor that runs Zith programs from the typed HIR without lowering
to a native object/executable. It is the portable execution path when LLVM is not
available and the baseline for future bytecode or comptime execution.
_Avoid_: runtime VM, HIR evaluator, fallback executor

**Runtime FFI handler**:
The internal table mapping a linkage name of an `extern fn` to a Zith-owned host
implementation, replacing host libc in portable/browser execution.
_Avoid_: C FFI, libc shim, native handler table

**Thread fork**:
The full-Zith core syntax `backend fork Entry(args)` that creates a thread through a backend object such as `pThread` and returns the backend's concrete `Thread<T>` handle.
_Avoid_: Thread capability, spawn keyword, ForkHandle

**Thread merge**:
The core keyword `merge handle` that blocks, consumes a `Thread<T>` handle once, and returns the entry result type declared by the fork.
_Avoid_: join, wait, release handle

**Thread spawn**:
The stdlib shorthand `spawn Entry(args)` that uses the active thread backend from a context; it is not core syntactic sugar in the compiler.
_Avoid_: core spawn keyword, runtime.spawn method

**Thread backend**:
The runtime capability/object that implements `ThreadBackend` and produces concrete thread handles for `fork`/`merge`, for example `pThread`.
_Avoid_: Branch capability, thread allocator, fork factory

**Thread handle**:
An owned, single-consumer value satisfying the `Thread<T>` protocol. `merge` consumes it exactly once; backend concrete handles may expose extra methods such as `detach`.
_Avoid_: ForkHandle, task, future

**Playground runtime**:
The single WASM module export (`zith_run_hir`/run path) that executes programs in the
browser by invoking the interpreter over the module compiled by the same artifact.
_Avoid_: separate VM module, compiler-only WASM, playground backend

**Host runtime error**:
The separate runtime failure channel for program execution (extern missing, panic,
division by zero) that is distinct from compiler diagnostics and from the program exit
status. Reuses `host_write` for output, but does not reuse `zith_last_error` as the only
error surface.
_Avoid_: compiler error, child exit status, runtime diagnostic

## Standard Library

**Formatable**:
The trait implemented by values that can be rendered through `print`/`println`.
_Avoid_: Printable, displayable, serializer

**FormatBuffer**:
The backing object supplied to `Formatable.format(self, dest)` so a value can append its rendered text.
_Avoid_: string builder, write buffer, output stream

**InputLine**:
The wrapper returned by `input()`. It owns the read buffer, exposes the trimmed line through methods, and supports `cast<T>`.
_Avoid_: input string, result line, readline result

**ParseInput**:
The trait implemented by primitive input types that can be parsed from an `InputLine`; a failed parse returns `null`.
_Avoid_: parser, conversion trait, strconv

**IoError**:
The result type used by `print`/`println` (and potentially `input`) for allocation and write failures in Zith--.
_Avoid_: error union, result error, failable error

**Consume**:
The ownership convention for resource cleanup: `destroy(self: lend)` owns and consumes the value, calling `free`/release internally.
_Avoid_: destructor, drop, free method

**Primitive erasure**:
Erasing a primitive value to a `dyn Trait` fat pointer so it can be handled through the same dynamic-dispatch surface as structs.
_Avoid_: boxing, wrapping, vtbl primitive

## Zith Proof Kernel (ZPK)

**Zith Proof Kernel (ZPK)**:
The umbrella contract for the four future Zith proof sub-systems NIA, RRA, MRA and NRA.
`ZPK` is not one pass; it names the coordinated set and their shared facts.
_Avoid_: safety module, proof model, compiler analyzers

**NIA**:
Numeric Interval Analysis, the ZPK sub-system that collects and proves numeric facts, ranges, versions, control-flow joins and function headers.
_Avoid_: NTA, numeric analysis, facts module

**RRA**:
Region Relationship Analysis, the ZPK sub-system that proves geometry for slices, arrays, region ranges, pool slots and allocation blocks.
_Avoid_: geometry pass, region checker

**MRA**:
Memory Region Analysis, the ZPK sub-system that declares and controls static region, heap and pool shapes, permissions, stream typing and allocator provenance.
_Avoid_: memory map, address checker, allocator runtime

**NRA**:
Node Resource Analysis, the ZPK sub-system that proves ownership, lifetime, borrow and escape facts for resources.
_Avoid_: borrow checker, ownership checker

**Region**:
A static MRA declaration of contiguous memory with a known or symbolic shape, used for hardware, arenas and scratch memory.
_Avoid_: memory area, mmap, heap

**Heap**:
A separate MRA keyword for a dynamic allocation domain. The base may be `unknown` at compile time and the total size is `dynamic`; allocated blocks carry their own length.
_Avoid_: region, arena, malloc domain

**Pool**:
A static MRA declaration for a fixed-count typed region. `P`, `T`, and `N` derive `size`, `stride`, `alignment`, and slot facts; the declaration should not repeat derivable fields.
_Avoid_: typed region, block allocator region, object arena

**Block<R>**:
An allocation result carrying a region-provenance pointer and a byte length: `Block<R> { ptr: Ptr<R>, len: u64 }`.
_Avoid_: pointer with size, memory block, raw allocation

**Ptr<R>**:
A pointer object whose provenance is a comptime MRA region, heap, or pool named `R`.
_Avoid_: raw pointer, tagged pointer, region pointer

**Slot access**:
The MRA access to a fixed-count `pool P(T, N)` through `@poolSlot(P, index)`. The compiler derives `index < count` from the declaration, so RRA can prove `Disjoint(slot(k1), slot(k2))` when NIA proves `k1 != k2`.
_Avoid_: pool indexing, raw slot arithmetic, block slot

**Region access result**:
`@regionAt` may return `Ptr<R>` for a unique element or `Block<R>`/slice for an interval. A `Ptr` is the identity of one element; a `Block`/slice is an interval of elements/bytes.
_Avoid_: pointer, access span, region value

**Init**:
The single-site MRA transition that resolves a region/heap/pool with `base: unknown` to a runtime address and records its domain size; it is performed by `@regionInit(region, addr, size)`.
_Avoid_: constructor, setup, initialize permission

**Region access**:
The MRA expression that reads or writes a region through `@regionAt(region, offset)`. Typed streams follow their declared element type; untyped streams behave as raw byte streams.
_Avoid_: pointer arithmetic, raw cast, device access

**Bump no-op free**:
The bump allocator contract where `free(block)` does not reclaim the block. NRA treats the block as valid until region reset/scope end, not as freed.
_Avoid_: bump free, arena free, individual deallocation

**Contract intrinsic**:
One of `@assume`, `@ensure`, or `@maybe`: a ZPK-level construct that lets programmers and allocator implementations declare proof boundaries without blurring subsystem responsibilities.
_Avoid_: assertion, compiler hint, proof pragma

**Assume**:
The trusted-premise intrinsic `@assume(cond)`. Unless the current environment already proves `cond` false, installs `cond` as `True` in that scope; a known contradiction is a diagnostic. It is an audit-marked escape hatch, not a verified assertion.
_Avoid_: verify, assertion, assume if proved

**Ensure**:
The caller-side contract intrinsic `@ensure(cond)`. A function/capability declares that any caller must satisfy `cond` before the call; ZPK checks or derives the caller premise and records it as a contract fact.
_Avoid_: postcondition, guarantee, verifier promise

**Maybe gate**:
The intrinsic `@maybe(cond)`, valid only when the current analysis state for `cond` is exactly `Maybe`. `Unknown`, `True`, and `False` are rejected; the caller can consume the fuzzy fact conservatively.
_Avoid_: fallback predicate, optional condition, unknown check

**Cached premise header**:
The collected, cached contract facts for a function or capability. Normal callers verify these premises at the call site instead of reanalyzing the callee; each ZPK sub-system checks only the premises in its own domain.
_Avoid_: function annotation, inline contract, global proof cache

## Opaque Identity

**Canonical type id**:
The stable 128-bit identity of a Zith type, derived from its defining module namespace, canonical field order, and type name, used to hydrate `opaque` values.
_Avoid_: typeId, tag hash, deterministic hash id

**Runtime opaque tag**:
The project-local `u32` assigned by the compiler for a canonical type id and stored in the opaque runtime representation.
_Avoid_: global type id, canonical runtime id, stable tag

**Canonical type registry**:
The persistent project cache file that records which canonical type ids already have runtime opaque tags, used to keep later builds stable.
_Avoid_: global registry, runtime type table, tag table

**Canonical opaque mapping**:
The serialized per-artifact pair `{ canonical type id, runtime opaque tag }` that lets a cached opaque value be hydrated with its original tag.
_Avoid_: opaque metadata, tag recovery record, cache typehash

## Toolchain & C Interop

**Standalone toolchain**:
The packaging model where `zithc` carries the tools it needs to build native programs, so an end user installs only the compiler artifact instead of a separate host compiler toolchain.
_Avoid_: bundled compilers, host toolchain, toolchain folder

**Embedded LLD**:
The LLD linker linked into `zithcLib` and invoked through the LLD API in-process instead of executing `ld.lld` from the host.
_Avoid_: system linker, external LLD, ld.lld driver

**Host C driver**:
The external C compiler used as a temporary fallback for `*.c` compilation until Zith ships its own lightweight C backend.
_Avoid_: system compiler, user compiler, C toolchain

**Zith C binder**:
The future embedded, Zith-owned parser that consumes C headers and produces the same `cinterop` binding surface libclang produces today, without requiring LLVM/libclang.
_Avoid_: C parser, libclang replacement, mini-clang

**Tiny C backend**:
The long-term lightweight C compiler/object emitter owned by Zith for companion C sources and for standalone C system-header support. It prioritizes small footprint, stability, and correct simple C over optimization.
_Avoid_: TinyCC backend, tinyc clone, mini C compiler

**Validated C ABI surface**:
The exact set of C declarations Zith promises to bind with correct ABI. Declarations outside this surface are rejected with a clear diagnostic, not imported with unverified semantics.
_Avoid_: arbitrary C interop, experimental C import, best-effort ABI

**WASI header bundle**:
The small set of C library headers shipped with the WASM build so `import "foo.h"` works in the browser without relying on host system headers.
_Avoid_: system headers for WASM, browser libc headers, precompiled headers

**Bundle provider**:
The configurable source of embedded/locally installed compiler resources (LLD, header resource dirs, Zith-owned C backend, WASI headers) resolved relative to the `zithc` binary.
_Avoid_: toolchain root, LLVM_DIR, compiler path
