// impl/cli/cli11_init.cpp — Forces CLI11 vtable emission for MinGW
//
// CLI11 is header-only but certain virtual classes (Formatter, ConfigBase)
// have non-inline virtual destructors. On MinGW the vtables don't get emitted
// unless a translation unit actually constructs or references them.
#include <CLI/CLI.hpp>

// Force vtable emission by constructing instances and referencing
// virtual methods in a way the compiler cannot optimize away.
namespace {
    struct Cli11VtableInit {
        Cli11VtableInit() {
            CLI::Formatter f;
            CLI::App give_me_a_name;

            const CLI::ConfigBase c;
            (void)c;
        }
    };
    const Cli11VtableInit g_init;
}
