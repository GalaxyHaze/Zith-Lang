#pragma once

#include "frontend/source/span.hpp"
#include <string>
#include <vector>

namespace zith::diagnostics::model {

    struct LabeledSpan {
        frontend::Span span;
        std::string label;
    };

    struct Note {
        frontend::Span span;
        std::string message;
    };

}
