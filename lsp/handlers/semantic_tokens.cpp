#include "handler_interface.h"
#include "../document_manager.h"
#include <zith/zith.hpp>
#include <iostream>
#include <vector>
#include <algorithm>

using json = nlohmann::json;

enum class SemanticTokenType : uint8_t {
    Keyword = 0,
    String = 1,
    Number = 2,
    Comment = 3,
    Operator = 4,
    Function = 5,
    Struct = 6,
    Variable = 7,
    Parameter = 8,
    Type = 9,
    Namespace = 10,
};

struct SemanticToken {
    size_t line;
    size_t start;
    size_t length;
    SemanticTokenType type;
};

static bool tokenLess(const SemanticToken& a, const SemanticToken& b) {
    if (a.line != b.line) return a.line < b.line;
    return a.start < b.start;
}

static SemanticTokenType tokenTypeFromZith(ZithTokenType t) {
    switch (t) {
        case ZITH_TOKEN_IDENTIFIER:
        case ZITH_TOKEN_SCOPE:
        case ZITH_TOKEN_UNKNOWN:
        case ZITH_TOKEN_END:
            return SemanticTokenType::Keyword;

        case ZITH_TOKEN_TYPE:
        case ZITH_TOKEN_STRUCT:
        case ZITH_TOKEN_COMPONENT:
        case ZITH_TOKEN_ENUM:
        case ZITH_TOKEN_RAW:
        case ZITH_TOKEN_UNION:
        case ZITH_TOKEN_FAMILY:
        case ZITH_TOKEN_ENTITY:
        case ZITH_TOKEN_TRAIT:
        case ZITH_TOKEN_TYPEDEF:
        case ZITH_TOKEN_IMPLEMENT:
        case ZITH_TOKEN_FN:
        case ZITH_TOKEN_IMPORT:
        case ZITH_TOKEN_USE:
        case ZITH_TOKEN_CONTEXT:
        case ZITH_TOKEN_MACRO:
        case ZITH_TOKEN_EXPORT:
        case ZITH_TOKEN_FROM:
        case ZITH_TOKEN_AS:
        case ZITH_TOKEN_LET:
        case ZITH_TOKEN_VAR:
        case ZITH_TOKEN_AUTO:
        case ZITH_TOKEN_CONST:
        case ZITH_TOKEN_MUTABLE:
        case ZITH_TOKEN_GLOBAL:
        case ZITH_TOKEN_COMPTIME:
        case ZITH_TOKEN_LEND:
        case ZITH_TOKEN_SHARED:
        case ZITH_TOKEN_VIEW:
        case ZITH_TOKEN_UNIQUE:
        case ZITH_TOKEN_EXTENSION:
        case ZITH_TOKEN_YIELD:
        case ZITH_TOKEN_ASYNC:
        case ZITH_TOKEN_FLOWING:
        case ZITH_TOKEN_ENTRY:
        case ZITH_TOKEN_NORETURN:
        case ZITH_TOKEN_RECURSE:
        case ZITH_TOKEN_MODIFIER:
        case ZITH_TOKEN_IF:
        case ZITH_TOKEN_ELSE:
        case ZITH_TOKEN_FOR:
        case ZITH_TOKEN_IN:
        case ZITH_TOKEN_SWITCH:
        case ZITH_TOKEN_RETURN:
        case ZITH_TOKEN_BREAK:
        case ZITH_TOKEN_CONTINUE:
        case ZITH_TOKEN_GOTO:
        case ZITH_TOKEN_MARKER:
        case ZITH_TOKEN_SCENE:
        case ZITH_TOKEN_SPAWN:
        case ZITH_TOKEN_AWAIT:
        case ZITH_TOKEN_JOIN:
        case ZITH_TOKEN_TRY:
        case ZITH_TOKEN_CATCH:
        case ZITH_TOKEN_MUST:
        case ZITH_TOKEN_THROW:
        case ZITH_TOKEN_DO:
        case ZITH_TOKEN_DROP:
        case ZITH_TOKEN_REQUIRE:
        case ZITH_TOKEN_IS:
        case ZITH_TOKEN_PREFIX:
        case ZITH_TOKEN_SUFIX:
        case ZITH_TOKEN_INFIX:
        case ZITH_TOKEN_NULL:
            return SemanticTokenType::Keyword;

        case ZITH_TOKEN_STRING:
        case ZITH_TOKEN_CHAR:
            return SemanticTokenType::String;

        case ZITH_TOKEN_NUMBER:
        case ZITH_TOKEN_FLOAT:
        case ZITH_TOKEN_HEXADECIMAL:
        case ZITH_TOKEN_BINARY:
        case ZITH_TOKEN_OCTAL:
            return SemanticTokenType::Number;

        case ZITH_TOKEN_AND:
        case ZITH_TOKEN_OR:
        case ZITH_TOKEN_NOT_EQUAL:
        case ZITH_TOKEN_EQUAL:
        case ZITH_TOKEN_GREATER_THAN_OR_EQUAL:
        case ZITH_TOKEN_LESS_THAN_OR_EQUAL:
        case ZITH_TOKEN_ARROW:
        case ZITH_TOKEN_PLUS_EQUAL:
        case ZITH_TOKEN_MINUS_EQUAL:
        case ZITH_TOKEN_MULTIPLY_EQUAL:
        case ZITH_TOKEN_DIVIDE_EQUAL:
        case ZITH_TOKEN_DECLARATION:
        case ZITH_TOKEN_DOTS:
        case ZITH_TOKEN_PLUS:
        case ZITH_TOKEN_MINUS:
        case ZITH_TOKEN_MULTIPLY:
        case ZITH_TOKEN_DIVIDE:
        case ZITH_TOKEN_MOD:
        case ZITH_TOKEN_ASSIGNMENT:
        case ZITH_TOKEN_LESS_THAN:
        case ZITH_TOKEN_GREATER_THAN:
        case ZITH_TOKEN_BANG:
        case ZITH_TOKEN_QUESTION:
        case ZITH_TOKEN_PIPE:
            return SemanticTokenType::Operator;

        default:
            return SemanticTokenType::Keyword;
    }
}

static void scanComments(const std::string& source, std::vector<SemanticToken>& tokens) {
    size_t i = 0;
    while (i < source.size()) {
        if (source[i] == '/' && i + 1 < source.size()) {
            if (source[i + 1] == '/') {
                size_t start = i;
                size_t line = 0;
                for (size_t j = 0; j < start; ++j) {
                    if (source[j] == '\n') ++line;
                }
                while (i < source.size() && source[i] != '\n') ++i;
                tokens.push_back({line, start, i - start, SemanticTokenType::Comment});
                continue;
            }
            if (source[i + 1] == '*') {
                size_t start = i;
                size_t line = 0;
                for (size_t j = 0; j < start; ++j) {
                    if (source[j] == '\n') ++line;
                }
                i += 2;
                while (i + 1 < source.size() && !(source[i] == '*' && source[i + 1] == '/')) {
                    if (source[i] == '\n') ++line;
                    ++i;
                }
                if (i + 1 < source.size()) i += 2;
                tokens.push_back({line, start, i - start, SemanticTokenType::Comment});
                continue;
            }
        }
        ++i;
    }
}

json getSemanticTokens(DocumentManager& docManager, const std::string& uri) {
    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return json::object();
    }

    const std::string& content = doc->getContent();

    std::vector<SemanticToken> tokens;

    try {
        zith::Arena arena;
        ZithTokenStream stream = zith::tokenize(arena, content);

        for (size_t i = 0; i < stream.len; ++i) {
            const auto& tok = stream.data[i];
            if (tok.type == ZITH_TOKEN_IDENTIFIER) continue;

            if (tok.type >= ZITH_TOKEN_LPAREN && tok.type <= ZITH_TOKEN_PIPE) continue;

            SemanticTokenType type = tokenTypeFromZith(tok.type);
            size_t line = tok.loc.line > 0 ? tok.loc.line - 1 : 0;
            tokens.push_back({line, tok.loc.index, tok.lexeme.len, type});
        }
    } catch (const std::exception& e) {
        std::cerr << "Error tokenizing for semantic tokens: " << e.what() << std::endl;
    }

    scanComments(content, tokens);

    std::sort(tokens.begin(), tokens.end(), tokenLess);

    std::vector<int> encoded;
    size_t prevLine = 0;
    size_t prevStart = 0;

    for (const auto& tok : tokens) {
        int deltaLine = static_cast<int>(tok.line - prevLine);
        int deltaStart;
        if (deltaLine == 0) {
            deltaStart = static_cast<int>(tok.start - prevStart);
        } else {
            deltaStart = static_cast<int>(tok.start);
        }
        encoded.push_back(deltaLine);
        encoded.push_back(deltaStart);
        encoded.push_back(static_cast<int>(tok.length));
        encoded.push_back(static_cast<int>(tok.type));
        encoded.push_back(0);
        prevLine = tok.line;
        prevStart = tok.start;
    }

    json result;
    result["data"] = encoded;
    return result;
}
