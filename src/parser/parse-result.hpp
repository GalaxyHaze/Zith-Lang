#pragma once

#include "memory/result.hpp"
#include "ast/ast-nodes.hpp"


namespace zith::parser {

    using ProgramResult = memory::Result<ast::ProgramNode>;

} // namespace zith::parser
