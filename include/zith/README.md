# include/zith

Headers públicos do compilador/SDK Zith.

## Organização da API pública

- `zith.hpp`: façade agregadora para compatibilidade e ponto único de include.
- `token.hpp`: tokens, tipos-base de source (`ZithSourceLoc`, `ZithStr`, stream de tokens) e lexer.
- `ast.hpp`: nós da AST e IDs de nós.
- `parser.hpp`: parsing e parsing com contexto de import.
- `memory.hpp`: arena allocator pública.
- `import.hpp`: utilitários de arquivo/import usados pela superfície pública.
- `diagnostics.hpp`: códigos de erro públicos.

## Fronteira pública vs interna

- Tudo em `include/zith/*.hpp` (exceto `include/zith/impl/**`) é **API pública**.
- Headers em `include/zith/impl/**` são **internos** e não fazem parte de contrato estável.
- Consumidores externos devem incluir apenas `zith.hpp` ou headers granulares públicos.

## Convenções de estabilidade

### Estável (API/ABI)

- Assinaturas de funções C exportadas nestes headers públicos.
- Layout de structs públicos (`ZithToken`, `ZithTokenStream`, `ZithNode`, etc.).
- Valores numéricos de enums/tags já publicados que impactem serialização, FFI ou plugins.

Mudanças nessas áreas exigem versionamento explícito e nota de migração.

### Experimental

- Qualquer símbolo marcado futuramente com prefixo/sufixo `experimental` ou macro de feature.
- Conteúdo em `include/zith/impl/**`.
- Helpers C++ de conveniência no namespace `ZITH` podem evoluir mais rápido, desde que não quebrem a camada C subjacente.

## Diretriz de dependência

- Código interno pode depender da API pública.
- API pública **não** deve expor tipos privados de `impl/`.
- Evitar que headers públicos incluam headers de `impl/` direta ou indiretamente.
