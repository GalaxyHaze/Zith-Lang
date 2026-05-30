#pragma once

#include "frontend/source/span.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace zith::diagnostics::model {

    enum class Severity : uint8_t {
        Note,
        Warning,
        Error,
        Bug
    };

    struct Label {
        frontend::Span span;
        std::string message;
    };

    struct Diagnostic {
        Severity severity;
        uint32_t code;
        std::string message;
        frontend::Span primary;
        std::vector<Label> labels;

        bool isError() const noexcept;
        bool isWarning() const noexcept;
    };

}
