# macOS Builds: Build It Yourself

We do not ship macOS binaries.

The macOS target needs the Apple SDK, and that SDK cannot be redistributed or
obtained legally outside Apple's own terms. We will not ship an SDK, and we will
not ship a macOS artifact built against one we cannot legally distribute.

If you want Self-Zith on macOS, please build it yourself:

1. On a Mac, install Xcode Command Line Tools (which provides the SDK).
2. Build with the system toolchain: `cmake -S . -B build && cmake --build build -j`.

Maintainers who want to cross-build macOS from Linux can set up `osxcross`
against an SDK they obtained on a Mac themselves. That is a local convenience,
not something this repository provides or distributes.

This is a licensing constraint, not a technical gap: the compiler core is
target-agnostic (see [ADR-0012](../adr/0012-os-sensitive-code-is-a-c-layer.md)),
so porting is a matter of the SDK, not of the code.
