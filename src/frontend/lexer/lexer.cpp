#include "lexer.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/source/source-file.hpp"
#include "frontend/source/source-map.hpp"
#include "frontend/source/span.hpp"
#include "infra/util/result.hpp"
#include <cctype>
#include <cstdint>
#include <string_view>
#include <vector>

namespace zith::frontend::lexer {

    enum class ParserError: uint8_t {
        Successs = 0,
        UnexpectedToken,
        MissingSemicolon,
        InvalidNumber,
        UnexpectedEOF
    };

    static SourceLoc file; static FileId gId = 0;
    static const char *start = nullptr, *now = nullptr, *end = nullptr;
    static Loc loc;  static ParserError pError = ParserError::Successs;
    static std::vector<Token> tokens = {};

    static bool Ok(){
        return pError == ParserError::Successs;
    }

    static bool isNum(const char c){
        return c >= '0' && c <= '9';
    }

    #include <stdbool.h>

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
        std::string locationPrefix = "[error]: at" + file.path + ":" + loc.toString() + ": ";

        switch (pError) {
            case ParserError::Successs:
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

    static char peek(size_t n = 1){
        return (now + n <= end)? now[n] : '\0'; 
    }

    static bool consume(size_t offset){
        if ( end - now < offset )
            return false;
        now += offset;
        return true;
    }

    static bool match(std::string_view must){
        if ( end - now < must.size() )
            return false;
        std::string_view rest {now, end };
        if (rest.starts_with(must)){
            now += must.size();
            return true;
        }
        return false;

    }

    static void singleComment(){
        auto before = now - 2;
        while (Open()) {
            if (*now == '\n'){
                loc.col = 1;
                loc.line++;
                break;
            }
            loc.col++;
        }
        tokens.emplace_back(Span{gId, (uint32_t)(before - start), (uint32_t)(now - start)}, TokenKind::Comments );

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

    

    constexpr auto tokenize(const FileId id) -> infra::util::Result<TokenStream>{
        gId = id;
        std::vector<Token> vec;
        auto file = SourceMap::get(id);

        if (!file)
            return infra::util::Error{"[error] Couldnt find the file"};

        auto content = file.value().get().getSlice();
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
        vec.emplace_back(Span{0,0,0}, TokenKind::End);
        return TokenStream{};

        //return {nullptr, 0, 0, 0};
    }

}