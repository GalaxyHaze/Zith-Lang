#include "lexer.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/source/source-file.hpp"
#include "frontend/source/source-map.hpp"
#include "frontend/source/span.hpp"
#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "infra/util/result.hpp"
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <string_view>
#include <vector>

namespace zith::frontend::lexer {

    enum class ParserError: uint8_t {
        Success = 0,
        UnexpectedToken,
        MissingSemicolon,
        InvalidNumber,
        UnexpectedEOF
    };

    static SourceLoc* file; static FileId gId = 0;
    static const char *start = nullptr, *now = nullptr, *end = nullptr;
    static Loc loc;  static auto pError = ParserError::Success;
    static infra::collections::DynArray<Token> tokens(infra::alloc::SessionArena);

    static bool Ok(){
        return pError == ParserError::Success;
    }

    static bool isNum(const char c){
        return c >= '0' && c <= '9';
    }

    static bool isPunctuation(const char c) {
        switch (c) {
            // Brackets and Parentheses
            case '(': case ')':
            case '[': case ']':
            case '{': case '}':

            // Operators and Punctuation
            case '+': case '-': case '*': case '/': case '%':
            case '=': case '<': case '>': case '!':
            case '&': case '|': case '^': case '~':
            case '?': case ':': case ';': case ',': case '.':

            // Quotes and Misc Strings
            case '"': case '\'': case '\\': case '#': case '@': case '`':
                return true;

            default:
                return false;
        }
    }


    std::string getErrorMsg() {
        std::string locationPrefix = "[error]: at" + file->path + ":" + loc.toString() + ": ";

        switch (pError) {
            case ParserError::Success:
                return "";
            case ParserError::UnexpectedToken:
                return locationPrefix + "Unexpected token found.";
            case ParserError::MissingSemicolon:
                return locationPrefix + "Missing semicolon ';'.";
            case ParserError::InvalidNumber:
                return locationPrefix + "Invalid number format.";
            case ParserError::UnexpectedEOF:
                return locationPrefix + "Unexpected End of File (EOF).";
            default:
                return locationPrefix + "Unknown parser error.";
        }
    }

    static bool Open(){
        return now < end;
    }

    static char peek(){
        return (now + 1 <= end)? now[1] : '\0'; 
    }

    static char peek(const size_t n){
        return (now + n <= end)? now[n] : '\0'; 
    }

    static bool consume(const size_t offset = 1){
        if ( end - now < offset )
            return false;
        now += offset;
        return true;
    }

    static bool match(const std::string_view must){
        if ( end - now < must.size() )
            return false;
        std::string_view rest {now, end };
        if (rest.starts_with(must)){
            now += must.size();
            return true;
        }
        return false;

    }

    static void singleComment() {
        const auto before = now - 2;
        while (Open()) {
            if (*now == '\n'){
                loc.col = 1;
                loc.line++;
                break;
            }
            loc.col++;
        }
        tokens.emplace(Span{gId, static_cast<uint32_t>(before - start), static_cast<uint32_t>(now - start)}, TokenKind::Comments );

    }

    static void singleDoc(){

    }

    static void multiComment(){

    }

    static void multiDoc(){

    }

    static void processNumber(){

    }

    static void processString(){

    }

    static void processIdentifier(){

    }

    

    auto tokenize(const FileId id) -> infra::util::Result<TokenStream>{
        gId = id;
        if ( auto i = SourceMap::get(id) ) {
            file = &i.value().get();
        }

        if (!file)
            return infra::util::Error{"[error] Couldnt find the file"};

        const auto content = file->getSlice();
        start = now = content.data();
        end = content.data() + content.size();
        
        while( Open() ){

            if ( match("//") )
                singleComment();

            if ( match("///") )
                singleDoc();

            if ( match("/*") )
                multiComment();

            if ( match("/**") )
                multiDoc();

            if ( *now == '"')
                processString();

            if ( std::isspace(*now) )
                consume(1);

            if ( isNum(*now) )
                processNumber();
        }
        tokens.emplace(Span{id,loc.line,loc.col}, TokenKind::End);
        return TokenStream{};

        //return {nullptr, 0, 0, 0};
    }

    static const char* token_kind_name(TokenKind k) noexcept {
        switch (k) {
            case TokenKind::Identifier:   return "Identifier";
            case TokenKind::As:           return "As";
            case TokenKind::Using:        return "Using";
            case TokenKind::Type:         return "Type";
            case TokenKind::Struct:       return "Struct";
            case TokenKind::Raw:          return "Raw";
            case TokenKind::Must:         return "Must";
            case TokenKind::Mutable:      return "Mutable";
            case TokenKind::Trait:        return "Trait";
            case TokenKind::Interface:    return "Interface";
            case TokenKind::Typedef:      return "Typedef";
            case TokenKind::Implement:    return "Implement";
            case TokenKind::Fn:           return "Fn";
            case TokenKind::Module:       return "Module";
            case TokenKind::Extern:       return "Extern";
            case TokenKind::Macro:        return "Macro";
            case TokenKind::Context:      return "Context";
            case TokenKind::Variable:     return "Variable";
            case TokenKind::Ownership:    return "Ownership";
            case TokenKind::Yield:        return "Yield";
            case TokenKind::Label:        return "Label";
            case TokenKind::Visibility:   return "Visibility";
            case TokenKind::If:           return "If";
            case TokenKind::For:          return "For";
            case TokenKind::In:           return "In";
            case TokenKind::Match:        return "Match";
            case TokenKind::Control:      return "Control";
            case TokenKind::Scene:        return "Scene";
            case TokenKind::Thread:       return "Thread";
            case TokenKind::Error:        return "Error";
            case TokenKind::Drop:         return "Drop";
            case TokenKind::Require:      return "Require";
            case TokenKind::Is:           return "Is";
            case TokenKind::Word:         return "Word";
            case TokenKind::Logical:      return "Logical";
            case TokenKind::Comparision:  return "Comparision";
            case TokenKind::Operators:    return "Operators";
            case TokenKind::Comments:     return "Comments";
            case TokenKind::Docs:         return "Docs";
            case TokenKind::Annotation:   return "Annotation";
            case TokenKind::Punctuation:  return "Punctuation";
            case TokenKind::LitVal:       return "LitVal";
            case TokenKind::Unknown:      return "Unknown";
            case TokenKind::End:          return "End";
        }
        return "???";
    }

    void print_tokens(const TokenStream& stream, std::string_view source) noexcept {
        for (uint32_t i = 0; i < stream.len; ++i) {
            const auto& tok = stream.src[i];
            auto lexeme = TokenStream::getLexeme(tok, source);
            printf("  %-16s \"%.*s\"  [%u..%u]\n",
                   token_kind_name(tok.kind),
                   (int)lexeme.size(), lexeme.data(),
                   tok.span.start, tok.span.end);
        }
    }

}