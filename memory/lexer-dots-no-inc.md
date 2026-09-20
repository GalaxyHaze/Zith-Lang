# Lexer Dots Token and No Increment

The frontend lexer emits `TokenKind::Dots` for both `..` and `...`; the parser checks the lexeme to decide the meaning. This removed the old dot-by-dot assumptions in ranges, slices, pipe placeholders, relative imports, `mod(..)`, and variadic markers.

`++` and `--` are rejected in Zith-- with `E2010` and the message `use explicit assignment to an updated value`. Prefix forms are handled in `parseExpression` and postfix forms in the binary loop, so both produce the same controlled diagnostic.

The canonical specification is `docs/Zith--.md`; implementation notes are in `docs/Zith---implementation.md`. The decision is recorded in `docs/adr/0023-no-increment-and-dots-token.md`.
