#pragma once

#include "memory/span.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zith::diagnostics {

    enum class Severity : uint8_t { Note, Warning, Error, Bug };

    struct Label {
        memory::Span span;
        std::string message;
    };

    struct Diagnostic {
        Severity severity;
        uint32_t code;
        std::string message;
        memory::Span primary;
        std::vector<Label> labels;
        std::vector<std::string> suggestions;

        bool isError() const noexcept;
        bool isWarning() const noexcept;
    };

} // namespace zith::diagnostics
