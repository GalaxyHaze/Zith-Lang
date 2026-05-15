// impl/lexer/keywords.hpp — Keyword lookup (C++ internal)
#pragma once

#include "zith/keyword.h"
#include <string_view>

namespace zith::lexer {
    ZithTokenType lookup_keyword(std::string_view sv);
}
