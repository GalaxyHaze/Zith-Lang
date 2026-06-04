#include "diagnostic.hpp"

namespace zith::diagnostics::model {

    bool Diagnostic::isError() const noexcept {
        return severity == Severity::Error || severity == Severity::Bug;
    }

    bool Diagnostic::isWarning() const noexcept {
        return severity == Severity::Warning;
    }

} // namespace zith::diagnostics::model
