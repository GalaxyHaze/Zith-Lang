# Plano de Reestruturação Incremental do Compilador Zith

Cada fase é testável e compilável independentemente. Em caso de dúvida, pergunte antes de avançar.

---

## Fase 1 — Infraestrutura compartilhada (baixo risco, ~1-2 dias)

| # | O quê | Arquivo | Detalhes |
|---|---|---|---|
| 1.1 | Extrair `overloaded{}` | Novo: `src/common/overloaded.hpp` | Elimina duplicação em 4 arquivos |
| 1.2 | Adicionar `format()` | Novo: `src/common/format.hpp` | Substitui concatenação manual de strings |
| 1.3 | Accessors tipados `DeclNode` | Novo: `src/ast/ast-node-utils.hpp` | `asFn()`, `asStruct()`, `asEnum()` etc. |
| 1.4 | Pinar versão do mio | `CMakeLists.txt` | `GIT_TAG master` → tag específica |

**Teste:** Compila + 5 testes passam. Zero mudança de comportamento.

---

## Fase 2 — Variant ergonomics (risco médio, ~2-3 dias)

| # | O quê | Arquivos | Detalhes |
|---|---|---|---|
| 2.1 | Definir `DeclKind`, `ExprKind`, `StmtKind` | `src/ast/ast-ids.hpp` | Enums com tag |
| 2.2 | Adicionar campo `kind` às variantes | `src/ast/ast-nodes.hpp` | Cada nó carrega sua tag |
| 2.3 | Atualizar `AstBuilder` | `src/ast/ast-builder.hpp` + `.cpp` | `addDecl()` seta `kind` automaticamente |
| 2.4 | Refatorar `DeclNode` dispatch | `sema-pipeline.cpp`, `body-expand.cpp`, `resolver.cpp` | 13 `std::get_if` → switch on `kind` |
| 2.5 | Refatorar `ExprNode` dispatch | `sema-pipeline.cpp` | `if constexpr` chain → switch |
| 2.6 | Refatorar `HirExpr` dispatch | `codegen-emit.cpp` | `if constexpr` chain → switch |

**Teste:** Compila + 5 testes. Dispatch agora é exhaustivo via `-Wswitch`.

---

## Fase 3 — Import system decomposition (risco médio-alto, ~2-3 dias)

| # | Novo arquivo | Responsabilidade |
|---|---|---|
| 3.1 | `src/symbols/import-resolver.{hpp,cpp}` | `find_file()`, `resolve_directory()`, `collect_dir_files()`, `computeNamespace()` |
| 3.2 | `src/symbols/module-loader.{hpp,cpp}` | Lex → scan → expand de um arquivo |
| 3.3 | `src/symbols/dep-graph.{hpp,cpp}` | Dedup, ciclo detection, recursive walk |
| 3.4 | `src/symbols/symbol-merger.{hpp,cpp}` | `mergeInto()`, selective imports, origin tracking |
| 3.5 | Refatorar `ImportManager` | Thin orchestrator que chama as 4 classes |

---

## Fase 4 — Parser: eliminar duplicação scan/expand (risco alto, ~3-5 dias)

| # | O quê | Detalhes |
|---|---|---|
| 4.1 | Unificar `scanFieldItem()` | Lógica duplicada em scan.cpp + body-expand.cpp |
| 4.2 | Unificar `scanEnumVariant()` | Lógica de enum body duplicada |
| 4.3 | Unificar `scanUnionVariant()` | Lógica de union body duplicada |
| 4.4 | Decisão: two-pass com shared helpers OU single-pass | Usuário planeja operadores word definidos pelo usuário → expressões ficam como estão e são re-ordenadas por precedência no fim |

---

## Fase 5 — Sema: desacoplar type-checking de HIR (risco mais alto, ~3-5 dias)

| # | O quê | Detalhes |
|---|---|---|
| 5.1 | Criar `TypedAst` | Mapa `ExprId → TypeId` |
| 5.2 | Modificar `SemaPipeline` | Type-checking popular `TypedAst` em vez de emitir HIR |
| 5.3 | Criar `HirLower` | Pass separado consome `TypedAst` + `ProgramNode` |
| 5.4 | Refatorar pipeline | `compilation-session.cpp`: sema → lower |

---

## Estimativa de tempo

| Ritmo | Duração | Calendário |
|---|---|---|
| 7h/dia | 11-17 dias | ~2.5-3.5 semanas |
| 5h/dia | 16-24 dias | ~3.5-5 semanas |
| 3h/dia | 26-39 dias | ~5.5-8 semanas |
