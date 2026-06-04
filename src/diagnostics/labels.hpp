#pragma once

#include "parser/span.hpp"

#include <string>

namespace zith::diagnostics {

    struct LabeledSpan {
        parser::Span span;
        std::string label;
    };

    struct Note {
        parser::Span span;
        std::string message;
    };

} // namespace zith::diagnostics
