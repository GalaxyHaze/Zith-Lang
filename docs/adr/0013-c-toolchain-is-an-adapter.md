# The C toolchain is an adapter behind a seam

Compiling the C layer and linking are done through a toolchain seam with a
`ToolchainBackend` contract (native compile, link, header import, sysroot
discovery). Adapters are tried in a deterministic preference order:
clang + `--target` + `lld` with an explicit sysroot first, Zig (`zig cc`) second,
a native cross-GCC (such as Void's `cross-*` packages) third.

clang is already a cross-compiler (one binary, many targets); what a target
needs is a triple, a sysroot, a linker, and compiler-rt. Which source provides
those is an adapter detail, not a property of the core. A single hard-wired
toolchain was rejected: it would tie the project to one vendor's sysroot story
and block a hermetic distribution. The first adapter is clang + `lld`, which is
already present on the development host; Zig is the fallback for targets whose
sysroots are not worth maintaining by hand.
