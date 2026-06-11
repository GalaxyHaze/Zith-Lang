#pragma once

#include "ast/ast-nodes.hpp"
#include "memory/result.hpp"

namespace zith::parser {

using ProgramResult = memory::Result<ast::ProgramNode>;

} // namespace zith::parser
