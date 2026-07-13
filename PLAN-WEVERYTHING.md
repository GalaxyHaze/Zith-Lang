# Plano: Corrigir warnings do -Weverything

## Situação atual

O projeto não compila com `-Weverything`. São ~1.121 erros únicos (5.022 instâncias quando headers são expandidos).

## Estratégia: Suprimir vs. Corrigir

### A. Suprimir no CMakeLists.txt (inerente à arquitetura)

Estes warnings são do modelo de memória arena e infraestrutura C. Corrigi-los significaria reescrever DynArray/Arena/FlatMap.

```cmake
# Adicionar ao bloco ZITH_CLANG_WARNINGS:
-Wno-unsafe-buffer-usage           # 494 inst — arena/DynArray pointer arithmetic
-Wno-padded                        # 275 inst — padding informativo em structs
-Wno-unsafe-buffer-usage-in-libc-call  # 140 inst — fprintf/strcmp/fputs
-Wno-old-style-cast                # 74 inst — (int)size() para printf %.*s
```

### B. Corrigir no código (mudanças triviais)

| # | Warning | Qtd | Correção | Arquivos |
|---|---|---|---|---|
| B.1 | `-Wshadow-field-in-constructor` | 61 | Renomear params: `tok` → `tok_`, `bld` → `bld_` | `parser.hpp`, `terminal.hpp`, `diagnostic.hpp`, `symbol-table.hpp`, `source-file.hpp` |
| B.2 | `-Wnewline-eof` | 18 | Adicionar `\n` no final | `macros.hpp`, `solver.cpp`, `solver.hpp`, `cache-entry.cpp` |
| B.3 | `-Wextra-semi` | 15 | Corrigir macro `aSelf`: remover `;` da definição | `macros.hpp` |
| B.4 | `-Wdisabled-macro-expansion` | 14 | Trocar `#define` por `using` | `options.cpp` |
| B.5 | `-Wunused-parameter` | 8 | Prefixar com `_` ou remover | `options.cpp`, `scan.cpp` |
| B.6 | `-Wsign-conversion` | 8 | `static_cast<size_t>()` | `keyword-table.hpp`, `compilation-session.cpp` |
| B.7 | `-Wunsafe-buffer-usage-in-container` | 6 | Usar `std::span` direto | `diagnostic-engine.cpp`, `type-lower.cpp` |
| B.8 | `-Wformat-nonliteral` | 5 | `__attribute__((format(printf, ...)))` | `terminal.hpp` |
| B.9 | `-Wunused-macros` | 2 | Remover ou `#ifdef` | `cmd/info.cpp` |
| B.10 | `-Wundef` | 1 | `#elif defined(_WIN32)` | `cmd/fmt.cpp` |
| B.11 | `-Wmissing-prototypes` | 1 | Adicionar `static` | `lexer.cpp` |

### Ordem de execução

1. CMakeLists.txt — adicionar 4 exclusões
2. B.3 — corrigir macro `aSelf`
3. B.1 — renomear constructor params
4. B.2 — adicionar newlines
5. B.4 — trocar macro por using
6. B.5-B.11 — correções pontuais
7. Build + teste — zero warnings

### Resultado esperado

- ~45 warnings suprimidos via CMake (arquitetura)
- ~190 warnings corrigidos no código (triviais)
- ~480 warnings restantes de `-Wpadded` e `-Wunsafe-buffer-usage` (suprimidos)
- Build limpo: 0 erros, 0 warnings
