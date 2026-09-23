#pragma once

#include "diagnostics/error-codes.hpp"
#include "frontend/frontend.hpp"

namespace zith::frontend {

inline AttributeKind classifyAttribute(Attribute &attr, AttributeTarget target,
                                       std::vector<Diagnostic> &diagnostics) {
    AttributeKind classified = AttributeKind::Unknown;
    if (attr.text == "discardable" && target == AttributeTarget::Function)
        classified = AttributeKind::Discardable;
    if (attr.text == "volatile" && target == AttributeTarget::Variable)
        classified = AttributeKind::Volatile;
    if (classified == AttributeKind::Unknown && attr.text != "discardable" &&
        attr.text != "volatile") {
        diagnostics.push_back({attr.span, "unknown attribute '" + attr.text + "'", true,
                               diagnostics::err::UnknownAttribute});
    } else if (classified == AttributeKind::Unknown) {
        diagnostics.push_back({attr.span, "attribute '" + attr.text + "' is not applicable here",
                               true, diagnostics::err::AttributeNotApplicable});
    }
    attr.kind = classified;
    return classified;
}

} // namespace zith::frontend
