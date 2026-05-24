#pragma once

#include "frontend/lexer/token.hpp"
namespace zith::frontend::lexer {

    constexpr TokenStream tokenize(std::string_view content);

}