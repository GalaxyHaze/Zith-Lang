#pragma once

#include "diagnostics/lazy_message.hpp"
#include "diagnostics/span.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zith::diag {

enum class DiagLevel : uint8_t {
    Fatal   = 0,
    Bug     = 1,
    Error   = 2,
    Warning = 3,
    Note    = 4,
    Help    = 5,
    Hint    = 6,
};

constexpr const char* diag_level_label(DiagLevel lvl) {
    switch (lvl) {
    case DiagLevel::Fatal:   return "fatal error";
    case DiagLevel::Bug:     return "internal compiler error";
    case DiagLevel::Error:   return "error";
    case DiagLevel::Warning: return "warning";
    case DiagLevel::Note:    return "note";
    case DiagLevel::Help:    return "help";
    case DiagLevel::Hint:    return "hint";
    default:                 return "unknown";
    }
}

constexpr bool is_error_level(DiagLevel lvl) {
    return lvl <= DiagLevel::Error;
}

enum class DiagCode : uint32_t {
    // ── Syntax / Parse (E0001–E0099) ──
    MissingSemicolon          = 0x0001,
    UnexpectedToken           = 0x0002,
    UnmatchedDelimiter        = 0x0003,
    ExpectedExpression        = 0x0004,
    ExpectedIdentifier        = 0x0005,
    ExpectedType              = 0x0006,
    InvalidLiteral            = 0x0007,
    UnterminatedString        = 0x0008,
    InvalidEscapeSequence     = 0x0009,
    UnexpectedEof             = 0x000A,

    // ── Declaration / Scope (E0100–E0199) ──
    DuplicateDefinition       = 0x0100,
    UndefinedIdentifier       = 0x0101,
    InvalidVisibility         = 0x0102,
    MismatchedArity           = 0x0103,
    InvalidImportPath         = 0x0104,
    CyclicImport              = 0x0105,

    // ── Type System (E0200–E0299) ──
    TypeMismatch              = 0x0200,
    CannotInferType           = 0x0201,
    InvalidCoercion           = 0x0202,
    UndefinedType             = 0x0203,
    MismatchedReturnType      = 0x0204,
    TypeAnnotationRequired    = 0x0205,
    InvalidGenericArgument    = 0x0206,
    TypeArityMismatch         = 0x0207,

    // ── Ownership / Mutability (E0300–E0399) ──
    ImmutableAssignment       = 0x0300,
    UseAfterMove              = 0x0301,
    BorrowConflict            = 0x0302,
    InvalidCapture            = 0x0303,
    OwnershipViolation        = 0x0304,

    // ── Control Flow (E0400–E0499) ──
    UnreachableCode           = 0x0400,
    MissingReturn             = 0x0401,
    UnusedResult              = 0x0402,
    InvalidBreakContinue      = 0x0403,

    // ── Warnings (W0001–W0099) ──
    UnusedVariable            = 0x8001,
    UnusedParameter           = 0x8002,
    UnusedImport              = 0x8003,
    ShadowedName              = 0x8004,
    DeadCode                  = 0x8005,
    DeprecatedSyntax          = 0x8006,
    UnnecessaryCast           = 0x8007,

    // ── Performance / Style Hints (W0100–W0199) ──
    InefficientAllocation     = 0x8100,
    RedundantCopy             = 0x8101,
    PreferConst               = 0x8102,

    // ── Internal (I0001–I0099) ──
    InternalCompilerError     = 0xF001,
    NotImplemented            = 0xF002,
};

constexpr const char* diag_code_string(DiagCode code) {
    switch (code) {
    case DiagCode::MissingSemicolon:          return "E0001";
    case DiagCode::UnexpectedToken:           return "E0002";
    case DiagCode::UnmatchedDelimiter:        return "E0003";
    case DiagCode::ExpectedExpression:        return "E0004";
    case DiagCode::ExpectedIdentifier:        return "E0005";
    case DiagCode::ExpectedType:              return "E0006";
    case DiagCode::InvalidLiteral:            return "E0007";
    case DiagCode::UnterminatedString:        return "E0008";
    case DiagCode::InvalidEscapeSequence:     return "E0009";
    case DiagCode::UnexpectedEof:             return "E0010";
    case DiagCode::DuplicateDefinition:       return "E0101";
    case DiagCode::UndefinedIdentifier:       return "E0102";
    case DiagCode::InvalidVisibility:         return "E0103";
    case DiagCode::MismatchedArity:           return "E0104";
    case DiagCode::InvalidImportPath:         return "E0105";
    case DiagCode::CyclicImport:              return "E0106";
    case DiagCode::TypeMismatch:              return "E0201";
    case DiagCode::CannotInferType:           return "E0202";
    case DiagCode::InvalidCoercion:           return "E0203";
    case DiagCode::UndefinedType:             return "E0204";
    case DiagCode::MismatchedReturnType:      return "E0205";
    case DiagCode::TypeAnnotationRequired:    return "E0206";
    case DiagCode::InvalidGenericArgument:    return "E0207";
    case DiagCode::TypeArityMismatch:         return "E0208";
    case DiagCode::ImmutableAssignment:       return "E0301";
    case DiagCode::UseAfterMove:              return "E0302";
    case DiagCode::BorrowConflict:            return "E0303";
    case DiagCode::InvalidCapture:            return "E0304";
    case DiagCode::OwnershipViolation:        return "E0305";
    case DiagCode::UnreachableCode:           return "E0401";
    case DiagCode::MissingReturn:             return "E0402";
    case DiagCode::UnusedResult:              return "E0403";
    case DiagCode::InvalidBreakContinue:      return "E0404";
    case DiagCode::UnusedVariable:            return "W0001";
    case DiagCode::UnusedParameter:           return "W0002";
    case DiagCode::UnusedImport:              return "W0003";
    case DiagCode::ShadowedName:              return "W0004";
    case DiagCode::DeadCode:                  return "W0005";
    case DiagCode::DeprecatedSyntax:          return "W0006";
    case DiagCode::UnnecessaryCast:           return "W0007";
    case DiagCode::InefficientAllocation:     return "W0101";
    case DiagCode::RedundantCopy:             return "W0102";
    case DiagCode::PreferConst:               return "W0103";
    case DiagCode::InternalCompilerError:     return "I0001";
    case DiagCode::NotImplemented:            return "I0002";
    default:                                  return "E????";
    }
}

struct Suggestion {
    SourceSpan span;
    std::string replacement;
    std::string label;
    bool is_machine_applicable;
};

struct LabeledSpan {
    SourceSpan span;
    LazyMessage label;
};

struct Diagnostic {
    DiagLevel level;
    DiagCode code;
    LazyMessage message;

    SourceSpan primary_span;
    bool has_primary_span = false;

    std::vector<LabeledSpan> secondary_spans;
    std::vector<Suggestion> suggestions;
    std::vector<std::unique_ptr<Diagnostic>> children;

    std::optional<DiagCode> caused_by;

    bool is_error() const { return is_error_level(level); }
    bool is_warning() const { return level == DiagLevel::Warning; }
    bool is_note() const { return level == DiagLevel::Note; }

    int priority_order() const {
        switch (level) {
        case DiagLevel::Fatal:   return 0;
        case DiagLevel::Bug:     return 1;
        case DiagLevel::Error:   return 2;
        case DiagLevel::Warning: return 3;
        case DiagLevel::Note:    return 4;
        case DiagLevel::Help:    return 5;
        case DiagLevel::Hint:    return 6;
        default:                 return 7;
        }
    }
};

} // namespace zith::diag
