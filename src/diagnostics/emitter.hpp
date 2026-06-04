#pragma once

#include "diagnostics/diagnostic-engine.hpp"

#include <string_view>

namespace zith::diagnostics {

    class Emitter {
        const DiagnosticEngine *engine_;

    public:
        explicit Emitter(const DiagnosticEngine &engine);

        void emit() const;
        void emitTo(std::string_view source_text) const;
    };

} // namespace zith::diagnostics
