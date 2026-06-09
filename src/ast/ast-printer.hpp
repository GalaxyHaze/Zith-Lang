#pragma once

#include "ast/ast-nodes.hpp"
#include "ast/ast-builder.hpp"

#include <cstdio>

namespace zith::ast {

    void printAST(const ProgramNode &program, const AstBuilder &builder, FILE *out = stdout);

} // namespace zith::ast
