#pragma once

#include "memory/result.hpp"
#include "ast/ast-ids.hpp"


namespace zith::parser {

    using ProgramResult = memory::Result<ast::DeclId>;

} // namespace zith::parser
