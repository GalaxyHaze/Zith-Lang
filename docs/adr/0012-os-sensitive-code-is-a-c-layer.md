# OS-sensitive code is isolated in a C layer

Every OS-sensitive operation (file system, process control, time, memory
mapping, terminal) sits in a C layer behind a narrow interface. The Zith-side
code talks to that interface, never to the platform directly.

C is the portability boundary because it is the one language every target has a
toolchain for, and because the project already needs a substantial C surface
(the C interop module and the runtime). Isolating the platform calls in C keeps
the compiler core target-agnostic: porting to a new OS means writing one adapter
for the C layer, not auditing the whole compiler. Spreading `#ifdef` platform
code through the Zith sources was rejected: it would put portability concerns in
every module and make the core untestable off-platform.
