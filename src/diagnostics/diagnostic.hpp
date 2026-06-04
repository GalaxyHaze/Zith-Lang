#pragma once

#include "parser/span.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zith::diagnostics {

    enum class Severity : uint8_t { Note, Warning, Error, Bug };

    struct Label {
        parser::Span span;
        std::string message;
    };

    struct Diagnostic {
        Severity severity;
        uint32_t code;
        std::string message;
        parser::Span primary;
        std::vector<Label> labels;

        bool isError() const noexcept;
        bool isWarning() const noexcept;
    };

} // namespace zith::diagnostics
