#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include <string_view>

namespace zith::diagnostics::engine {

    class Emitter {
        const DiagnosticEngine* engine_;

    public:
        explicit Emitter(const DiagnosticEngine& engine);

        void emit() const;
        void emitTo(std::string_view source_text) const;
    };

}
