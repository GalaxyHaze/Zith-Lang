#pragma once

#include <string_view>

namespace zith::diagnostics {

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
