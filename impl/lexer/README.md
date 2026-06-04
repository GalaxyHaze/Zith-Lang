# Lexer Layer

Converts raw source text into a stream of tokens.

## Files

```
lexer/
├── tokenizer.cpp    # Main tokenizer implementation
├── keywords.cpp     # Keyword detection
└── debug.h         # Debug utilities
```

## Pipeline

```
Source Text  ──►  Tokenizer  ──►  Token Stream
```

## Token Types

The lexer produces tokens in these categories:

### Literals
- `ZITH_TOKEN_STRING` — `"hello"`
- `ZITH_TOKEN_NUMBER` — `42`
- `ZITH_TOKEN_FLOAT` — `3.14`
- `ZITH_TOKEN_HEXADECIMAL` — `0xFF`
- `ZITH_TOKEN_BINARY` — `0b101`
- `ZITH_TOKEN_IDENTIFIER` — `my_var`

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Logical: `&&`, `||`, `!`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Assignment: `:=`, `+=`, `-=`, etc.

### Keywords
- `fn`, `struct`, `import`, `return`, `if`, `for`, `let`, `mut`
- Visibility: `pub`, `priv`, `prot`

### Special
- `(` `)` `[` `]` `{` `}` `,` `;` `:` `.` `->`
- `ZITH_TOKEN_EOF` — end of file
- `ZITH_TOKEN_ERROR` — invalid token

## Key Functions

```cpp
// Tokenize a source string
ZithTokenList zith_tokenize(const char *source, ZithArena *arena);

// Get next token (used by parser)
ZithToken zith_next_token(Tokenizer *t);

// Peek at next token without consuming
ZithToken zith_peek_token(Tokenizer *t);
```

## Error Handling

The lexer records errors in `DiagList` with source location (line, column). Maximum 50 errors before stopping.

## Integration

Tokens are consumed by `parser/` using `parser_peek()` and `parser_advance()` (see `parser_utils.cpp`).

## See Also

- `include/zith/zith.hpp` — Token enum definitions
- `parser/parser_utils.cpp` — Token consumption