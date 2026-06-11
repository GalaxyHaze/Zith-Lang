#pragma once

#include <cstdio>
#include <string_view>

namespace zith::diagnostics {

namespace ansi {
inline constexpr std::string_view reset = "\033[0m";
inline constexpr std::string_view bold  = "\033[1m";

inline constexpr std::string_view red     = "\033[31m";
inline constexpr std::string_view green   = "\033[32m";
inline constexpr std::string_view yellow  = "\033[33m";
inline constexpr std::string_view blue    = "\033[34m";
inline constexpr std::string_view magenta = "\033[35m";
inline constexpr std::string_view cyan    = "\033[36m";

inline constexpr std::string_view bright_red    = "\033[91m";
inline constexpr std::string_view bright_green  = "\033[92m";
inline constexpr std::string_view bright_yellow = "\033[93m";
inline constexpr std::string_view bright_cyan   = "\033[96m";
} // namespace ansi

struct ColorTheme {
    std::string_view severity_error;
    std::string_view severity_warning;
    std::string_view severity_bug;
    std::string_view severity_note;
    std::string_view location;
    std::string_view line_no;
    std::string_view underline;
    std::string_view help_prefix;
    std::string_view tip_prefix;
};

inline constexpr ColorTheme colorTheme = {
    .severity_error   = "\033[1;31m",
    .severity_warning = "\033[1;35m",
    .severity_bug     = "\033[1;91m",
    .severity_note    = "\033[1;36m",
    .location         = "\033[34m",
    .line_no          = "\033[36m",
    .underline        = "\033[32m",
    .help_prefix      = "\033[36m",
    .tip_prefix       = "\033[32m",
};

} // namespace zith::diagnostics
