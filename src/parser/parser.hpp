#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "lexer/token.hpp"
#include "parser/parse-result.hpp"
#include "parser/recovery.hpp"


namespace zith::parser {

    ProgramResult parseProgram(lexer::TokenStream tokens,
                                ast::AstBuilder &builder,
                                diagnostics::DiagnosticEngine &diags);

} // namespace zith::parser
