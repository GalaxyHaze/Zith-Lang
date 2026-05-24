#include "lexer.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/source/source-map.hpp"
#include "frontend/source/span.hpp"
#include "infra/util/result.hpp"
#include <string_view>
#include <vector>

namespace zith::frontend::lexer {

    static void processComment(const std::string_view content){
        Span span{0, 0, 0};

    }

    constexpr auto tokenize(const FileId id) -> infra::util::Result<TokenStream>{
        std::vector<Token> vec;
        auto file = SourceMap::get(id);
        if (!file)
            return infra::util::Error{"Place-holder"};
        //heuristic
        //auto size = file->content.size();
        //vec.reserve(size / 4);
        //for (size_t i = 0; i < size; i++){

        //}
        vec.emplace_back(Span{0,0,0}, TokenKind::End);


        //return {nullptr, 0, 0, 0};
    }

}