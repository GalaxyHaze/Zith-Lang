#pragma once

#include "frontend/lexer/token.hpp"
#include "frontend/source/span.hpp"
#include "infra/util/result.hpp"

namespace zith::frontend::lexer {

    auto tokenize(FileId id) -> infra::util::Result<TokenStream>;

    void print_tokens(const TokenStream& stream, std::string_view source) noexcept;

}