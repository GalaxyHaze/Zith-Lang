# macOS Compiler Build Policy (Proposed)

Status: proposed distribution policy for the bootstrap compiler. Revalidate
this policy and applicable SDK terms before making a release decision.

This project does not currently define or ship a macOS compiler artifact. The
proposal is to build on a Mac using the Apple toolchain and SDK, rather than
redistribute the SDK or an artifact whose distribution terms have not been
reviewed.

When a host build is available, document its actual command here. Do not use
the sibling compiler's CMake command as a build instruction for this repository.

Cross-building from Linux is outside the current bootstrap plan. Revisit it
after the host compiler and the C-to-Zith platform boundary are established;
see [the cross-compile strategy](../plans/cross-compile.md).
