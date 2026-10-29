# Package Lockfile

The package lockfile contract lives in ADR 0048 and the package-manager plan.
Keep lockfile paths project-relative for checkout portability. Validate raw
TOML path strings before constructing `std::filesystem::path`, which can
normalize repeated separators, and compare resolved roots with a key that
removes trailing separators because `lexically_normal()` can preserve them.
Give raw-string and `path` validators distinct names because `std::string`
arguments can make overload resolution ambiguous. `ZITH_LIB_SOURCES` uses
`file(GLOB_RECURSE)` without `CONFIGURE_DEPENDS`, so rerun CMake configuration
after adding a new source file.

SHA-256 appends the message bit length in big-endian order. The empty-input
vector cannot detect a reversed length field, so keep non-empty and multi-block
known vectors in the package tests.
