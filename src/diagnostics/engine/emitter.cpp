#include "emitter.hpp"
#include <cstdio>

namespace zith::diagnostics::engine {

    Emitter::Emitter(const DiagnosticEngine& engine)
        : engine_(&engine) {}

    void Emitter::emit() const {
        for (auto& d : engine_->all()) {
            std::fprintf(stderr, "[%s] %s\n",
                d.isError() ? "ERROR" : d.isWarning() ? "WARN" : "NOTE",
                d.message.c_str());
        }
    }

    void Emitter::emitTo(std::string_view source_text) const {
        (void)source_text;
        emit();
    }

}
