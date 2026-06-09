#pragma once

#include "memory/span.hpp"

#include <string>

namespace zith::diagnostics {

    struct LabeledSpan {
        memory::Span span;
        std::string label;
    };

    struct Note {
        memory::Span span;
        std::string message;
    };

} // namespace zith::diagnostics
