// include/zith/token.h — Tokenizer types and functions (C ABI)
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ZithTokenType {
    ZITH_TOKEN_IDENTIFIER = 0,

    ZITH_TOKEN_TYPE,
    ZITH_TOKEN_STRUCT,
    ZITH_TOKEN_COMPONENT,
    ZITH_TOKEN_ENUM,
    ZITH_TOKEN_RAW,
    ZITH_TOKEN_UNION,
    ZITH_TOKEN_FAMILY,
    ZITH_TOKEN_ENTITY,
    ZITH_TOKEN_TRAIT,
    ZITH_TOKEN_TYPEDEF,
    ZITH_TOKEN_IMPLEMENT,

    ZITH_TOKEN_FN,
    ZITH_TOKEN_IMPORT,
    ZITH_TOKEN_USE,
    ZITH_TOKEN_CONTEXT,
    ZITH_TOKEN_MACRO,
    ZITH_TOKEN_EXPORT,
    ZITH_TOKEN_FROM,
    ZITH_TOKEN_AS,

    ZITH_TOKEN_LET,
    ZITH_TOKEN_VAR,
    ZITH_TOKEN_AUTO,
    ZITH_TOKEN_CONST,
    ZITH_TOKEN_MUTABLE,
    ZITH_TOKEN_GLOBAL,
    ZITH_TOKEN_COMPTIME,
    ZITH_TOKEN_LEND,
    ZITH_TOKEN_SHARED,
    ZITH_TOKEN_VIEW,
    ZITH_TOKEN_UNIQUE,
    ZITH_TOKEN_EXTENSION,
    ZITH_TOKEN_YIELD,
    ZITH_TOKEN_ASYNC,
    ZITH_TOKEN_FLOWING,
    ZITH_TOKEN_ENTRY,
    ZITH_TOKEN_NORETURN,
    ZITH_TOKEN_RECURSE,

    ZITH_TOKEN_MODIFIER,

    ZITH_TOKEN_IF,
    ZITH_TOKEN_ELSE,
    ZITH_TOKEN_FOR,
    ZITH_TOKEN_IN,
    ZITH_TOKEN_SWITCH,
    ZITH_TOKEN_RETURN,
    ZITH_TOKEN_BREAK,
    ZITH_TOKEN_CONTINUE,
    ZITH_TOKEN_GOTO,
    ZITH_TOKEN_MARKER,
    ZITH_TOKEN_SCENE,

    ZITH_TOKEN_SPAWN,
    ZITH_TOKEN_AWAIT,
    ZITH_TOKEN_JOIN,

    ZITH_TOKEN_TRY,
    ZITH_TOKEN_CATCH,
    ZITH_TOKEN_MUST,
    ZITH_TOKEN_THROW,
    ZITH_TOKEN_DO,
    ZITH_TOKEN_DROP,

    ZITH_TOKEN_REQUIRE,
    ZITH_TOKEN_IS,
    ZITH_TOKEN_PREFIX,
    ZITH_TOKEN_SUFIX,
    ZITH_TOKEN_INFIX,

    ZITH_TOKEN_AND,
    ZITH_TOKEN_OR,
    ZITH_TOKEN_NOT_EQUAL,
    ZITH_TOKEN_EQUAL,
    ZITH_TOKEN_GREATER_THAN_OR_EQUAL,
    ZITH_TOKEN_LESS_THAN_OR_EQUAL,
    ZITH_TOKEN_ARROW,
    ZITH_TOKEN_PLUS_EQUAL,
    ZITH_TOKEN_MINUS_EQUAL,
    ZITH_TOKEN_MULTIPLY_EQUAL,
    ZITH_TOKEN_DIVIDE_EQUAL,
    ZITH_TOKEN_DECLARATION,
    ZITH_TOKEN_DOTS,

    ZITH_TOKEN_PLUS,
    ZITH_TOKEN_MINUS,
    ZITH_TOKEN_MULTIPLY,
    ZITH_TOKEN_DIVIDE,
    ZITH_TOKEN_MOD,
    ZITH_TOKEN_ASSIGNMENT,
    ZITH_TOKEN_LESS_THAN,
    ZITH_TOKEN_GREATER_THAN,
    ZITH_TOKEN_BANG,
    ZITH_TOKEN_QUESTION,

    ZITH_TOKEN_LPAREN,
    ZITH_TOKEN_RPAREN,
    ZITH_TOKEN_LBRACE,
    ZITH_TOKEN_RBRACE,
    ZITH_TOKEN_LBRACKET,
    ZITH_TOKEN_RBRACKET,
    ZITH_TOKEN_COMMA,
    ZITH_TOKEN_SEMICOLON,
    ZITH_TOKEN_COLON,
    ZITH_TOKEN_DOT,
    ZITH_TOKEN_PIPE,

    ZITH_TOKEN_NULL,
    ZITH_TOKEN_STRING,
    ZITH_TOKEN_NUMBER,
    ZITH_TOKEN_FLOAT,
    ZITH_TOKEN_HEXADECIMAL,
    ZITH_TOKEN_BINARY,
    ZITH_TOKEN_OCTAL,

    ZITH_TOKEN_SCOPE,
    ZITH_TOKEN_UNKNOWN,
    ZITH_TOKEN_END,
} ZithTokenType;

typedef struct ZithSourceLoc {
    size_t index;
    uint64_t line;
} ZithSourceLoc;

typedef struct ZithStr {
    const char *data;
    size_t len;
} ZithStr;

typedef struct ZithToken {
    ZithStr lexeme;
    ZithSourceLoc loc;
    ZithTokenType type;
    uint32_t keyword_id;
} ZithToken;

typedef struct ZithTokenStream {
    ZithToken *data;
    size_t len;
} ZithTokenStream;

typedef  struct ZithArena ZithArena;

ZithTokenStream zith_tokenize(ZithArena *arena, const char *source, size_t source_len);
void zith_debug_tokenize(ZithArena *arena, const char *source, size_t source_len);
const char *zith_token_type_name(ZithTokenType t);

#ifdef __cplusplus
}
#endif
