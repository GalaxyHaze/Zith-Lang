#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace zith::diagnostics {

using ErrCode = uint32_t;

namespace err {
// Lexical (1-999)
inline constexpr ErrCode UnknownToken      = 1;
inline constexpr ErrCode UnclosedString    = 2;
inline constexpr ErrCode InvalidEscape     = 3;
inline constexpr ErrCode InvalidIntLiteral = 4;
inline constexpr ErrCode UnclosedComment   = 5;

// Parse (1001-1999)
inline constexpr ErrCode ExpectedExpr       = 1001;
inline constexpr ErrCode ExpectedSemicolon  = 1002;
inline constexpr ErrCode UnclosedParen      = 1003;
inline constexpr ErrCode ExpectedIdent      = 1004;
inline constexpr ErrCode InvalidImportDepth = 1005;
inline constexpr ErrCode ImportError        = 1006;

// Semantic (2001-2999)
inline constexpr ErrCode UndefinedIdent = 2001;
inline constexpr ErrCode DuplicateDecl  = 2002;
inline constexpr ErrCode WrongArity     = 2003;
inline constexpr ErrCode UnusedDecl     = 2004;
inline constexpr ErrCode NotNamespace   = 2005;
inline constexpr ErrCode NoMember       = 2006;
inline constexpr ErrCode NoMatchingFn   = 2007;
inline constexpr ErrCode AmbiguousCall  = 2008;

// Types (3001-3999)
inline constexpr ErrCode TypeMismatch = 3001;
inline constexpr ErrCode CannotInfer  = 3002;
inline constexpr ErrCode InvalidCast  = 3003;
inline constexpr ErrCode CyclicType   = 3004;

// NRA / Ownership (4001-4999)
inline constexpr ErrCode UseAfterMove   = 4001;
inline constexpr ErrCode BorrowConflict = 4002;
inline constexpr ErrCode DoubleBorrow   = 4003;

// MIR / Lowering (5001-5999)
inline constexpr ErrCode InvalidIR   = 5001;
inline constexpr ErrCode Unreachable = 5002;

// Runtime (10001+)
inline constexpr ErrCode IndexOutOfBounds = 10001;
inline constexpr ErrCode DivisionByZero   = 10002;
inline constexpr ErrCode NullDeref        = 10003;
inline constexpr ErrCode Panic            = 10004;
} // namespace err

struct ErrorInfo {
    ErrCode code;
    char prefix;
    std::string_view category;
    std::string_view title;
    std::string_view tip;
};

std::optional<ErrorInfo> lookupError(ErrCode code) noexcept;

} // namespace zith::diagnostics
