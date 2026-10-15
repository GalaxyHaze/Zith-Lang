# Compilation Session Arena Lifetime

`CompilationSession::mProjectConfig` stores or uses allocator state from
`mScratchArena`. Declare `mScratchArena` before `mProjectConfig` in
`src/session/compilation-session.hpp` and preserve that order in the
constructor initializer list in `src/session/compilation-session.cpp`.

C++ constructs members in declaration order and destroys them in reverse
order. This keeps the arena alive while the project configuration is
constructed and destroyed. The dependency follows C++ Core Guidelines §C.13:
when member `B` uses member `A`, declare `A` before `B`.
