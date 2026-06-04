#pragma once

#include "parser/span.hpp"

#include <string>

namespace zith::diagnostics::model {

    struct LabeledSpan {
        frontend::Span span;
        std::string label;
    };

    struct Note {
        frontend::Span span;
        std::string message;
    };

} // namespace zith::diagnostics::model
