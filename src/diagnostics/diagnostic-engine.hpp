#pragma once

#include "diagnostic.hpp"
#include "diagnostics/color.hpp"
#include "diagnostics/diagnostic.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace zith::memory {
    class SourceMap;
}

namespace zith::diagnostics {

    class DiagnosticEngine {
        memory::DynArray<Diagnostic> diagnostics_;
        bool use_color_ = false;
        ColorTheme theme_ = colorTheme;
        memory::SourceMap *source_map_ = nullptr;

    public:
        void report(Diagnostic diag);
        void report(Severity sev, uint32_t code, std::string msg, memory::Span span);

        void setColor(bool enabled) { use_color_ = enabled; }
        bool useColor() const noexcept { return use_color_; }

        void setSourceMap(memory::SourceMap *sm) noexcept { source_map_ = sm; }
        memory::SourceMap *sourceMap() const noexcept { return source_map_; }

        void emit() const;
        void emitTo(std::string_view source_text) const;

        [[nodiscard]] bool hasErrors() const noexcept;
        [[nodiscard]] std::span<const Diagnostic> all() const noexcept;
        void clear();
    };

} // namespace zith::diagnostics
