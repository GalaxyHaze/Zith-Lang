#pragma once

#include "memory/dyn-array.hpp"
#include "common/span.hpp"

#include <cstdint>
#include <string>

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
    memory::DynArray<Label> labels;
    memory::DynArray<std::string> suggestions;

    Diagnostic(memory::Arena &arena, Severity sev, uint32_t code_, std::string msg,
               memory::Span span)
        : severity(sev), code(code_), message(std::move(msg)), primary(span), labels(arena),
          suggestions(arena) {}

    bool isError() const noexcept;
    bool isWarning() const noexcept;
};

} // namespace zith::diagnostics
