# Zith Implementation Debt

> Last updated: 2026-09-07.

Documento de gestão da dívida de implementação. Distingue propositadamente:

- **Dívidas reais**: funcionalidade que foi implementada de forma incompleta, com
  limitação conhecida que não é a intenção de design, ou código que repete lógica
  e dificulta manutenção.
- **Não-dívidas**: decisões de design intencionais para `Zith--`; manter o
  comportamento atual, mesmo que pareça incompleto comparado com o spec maior do
  Zith.

Este ficheiro não substitui `docs/impl-status.md`; é o inventário de trabalho de
engenharia para rever e gerir.

---

## Não-dívidas (decisões de design)

| Item | Decisão |
|---|---|
| `const fn` | Não é pretendido em `Zith--`. O parser aceita `FunctionKind::Const`, mas o pipeline rejeita `const fn` com `UnsupportedSyntax` em [frontend-decl.cpp](/home/diogo/Zith/src/frontend/frontend-decl.cpp:406). Não documentar como dívida. |
| `dyn Interface` sem acesso a fields | O design expõe apenas métodos em `dyn`; fields ficam disponíveis em tipos concretos e bounds genéricos. `a.x on dyn Interface` com `E3001` é comportamento pretendido, não debt. |

---

## Dívidas reais (features implementadas mas incompletas e código com risco)

### 1. `type Name = T` é nominal mas sem sintaxe completa

- Estado atual: `type Name = T` cria um wrapper nominal de um campo; não é
  intercambiável com `T`.
- Incompleto: não existe sintaxe explícita de construção e acesso. Existem casts
  de wrapper/unwrapper (`T as Name` / `Name as T`), mas a superfície é posta como
  `Partial` em [impl-status.md](/home/diogo/Zith/docs/impl-status.md:84).
- Decisão em aberto: definir sintaxe de construção/acesso ou declarar a forma
  atual suficiente para `Zith--`.

### 2. Cache ainda não usa `.zirl`

- Estado atual: o object cache funciona e realiza hits; o formato `.zirl` não é
  produzido nem consumido.
- Risco: estado completo do artefacto não é persistido numa representação estável;
  invalidações e round-trips dependem do array de object files.
- Referência: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:44).

### 3. NRA está parcial

- Estado atual: facts residuais e call annotations existem e são consumidos antes
  do lowering final.
- Faltas reais: o state machine completo alive/dead/lent e a prova de quatro
  regras não existem; não há todos os diagnósticos de ownership previstos.
- Referência: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:41).

### 4. Bare `opaque` usa hydration estável mas ainda depende de canonização consistente

- Estado atual: o typeId canónico é derivado do namespace do módulo, ordem
  canónica de fields e nome do tipo; tags project-local são
  serializadas no artefacto e re-hidratadas entre sessões de cache. O `E2010`
  só é reportado quando o tag canónico de um artefacto hidratado não bate com
  a atribuição nova da sessão.
- Dívida real: a estabilidade hidratada depende da canonização de todos os
  tipos importados/cacheados e da regra de canonical field order; mudar essa
  regra invalida tags antigos. Falta um registry mais explícito de typeIds
  cross-module que suporte evolução da canonização sem quebrar caches.
- `coerceValue` trata `opaque -> opaque` como no-op de sema, pelo que casts
  vindos de módulos importados não são rejeitados como erro de re-tagging.
- Referência: hydration e erro de instabilidade em
  [sema-cast-coerce.cpp](/home/diogo/Zith/src/sema/sema-cast-coerce.cpp),
  hydration em
  [compilation-session.cpp](/home/diogo/Zith/src/session/compilation-session.cpp:503)
  e testes em
  [test-hir-lower-modern.cpp](/home/diogo/Zith/tests/test-hir-lower-modern.cpp:1106).

### 5. C interop é `Working (validated C)`, não ABI completa

- Estado atual: libclang cobre C comum, variadics, parâmetros array-decayed,
  `va_list` e function pointers; object-like scalar macros são importadas como
  constantes.
- Dívida real: struct-by-value ABI é limitado a records simples cuja
  layout/alignment libclang prova para o target configurado; scalars, pointers
  e nested records verificados são suportados. Bitfields, packed/anonymous
  records, flexible arrays, globals, strings, function-like macros,
  `long double` e `__int128` não são importados.
- Referência: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:153).

### 6. Outras incompletudes registadas

- Literal ranges fazem `ExprKind::Range` e baixam a sema/control flow; falta
  ainda, como dívida residual, o tratamento completo de todas as formas
  `1..5`, `1>..5`, `1..<5`, `1>..<5` nas fronteiras do loop e do slicing.
- `is <type>` fora de unions/opaque não existe.
- Narrowing após `is null` / `not (is null)` para aggregate optionals (`?T`
  com payload não-pointer) extrai o campo 0 no then/else correto; `?*T -> *T`
  unchecked permanece para C pointers.
- Casts numéricos estreitantes não verificam overflow.
- `++` / `--` não existem.
- Formatter reimprime `for (cond)` como `while`.
- `..` é lexado caractere a caractere.

Estas entradas detalham o estado real e as referências de bloqueio; são as
mesmas lacunas da secção `Known Debt` de [impl-status.md](/home/diogo/Zith/docs/impl-status.md)
e devem ser consolidadas aqui quando forem tratadas.

### 7. Falhas conhecidas em `test-codegen`

Estado de 2026-09-04: `./build/test-codegen` reporta `370 passed, 0 failed`.
As seis falhas conhecidas de 2026-09-01 foram resolvidas:

- Pointer indexing inválido por ownership (`E4008`) foi corrigido nos dois
  verificações reportados.
- `return when (...)` sem ponto e vírgula passou a ser aceite apenas quando o
  `when` fecha com `}`; parser e testes foram ajustados.
- Qualified `lend`/`view` receivers (`E5001`) passam a gerar IR válido, e
  receivers de optional aggregate após `is null` / `not (is null)` extraem o
  payload antes de passar como `self`.
- `ownership-advanced.zith` executa com exit `16`, conforme esperado pelo
  exemplo.

### 8. `ParseInput` / `InputLine.cast<T>` está implementado, com `*char` fora de âmbito

- Estado: `ParseInput` e `InputLine.cast<T>` estão implementados em
  `std/io/console` para `bool`, `f32`, `f64`, `i32` e `u32`. A chamada
  `T.parse(self)` resolve através do bound da trait após monomorfização e
  `line.cast<NonParsable>()` reporta `E3009`.
- Não-dívida: `*char` fica intencionalmente fora do contrato actual; strings
  continuam disponíveis pelo adapter `text()` do `InputLine`. Adicionar
  parsing de outros primitivos é uma extensão opcional aos mesmos
  `implement ... as ParseInput`, não uma lacuna do contrato actual.
- Referência de estado: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:45)
  na linha `Stdlib I/O` e o plano completo em
  [parse-input-cast.old.md](/home/diogo/Zith/docs/plans/archive/parse-input-cast.old.md).

---

## Dívida de estrutura: monolitos

Os ficheiros abaixo ainda concentram demasiado pipeline por ficheiro. Já foram
concluídos, e estão fora da lista activa, os splits de
`src/session/frontend-context.cpp` e `src/session/compilation-session.cpp`.

| Ficheiro | Linhas atuais | Quebra proposta |
|---|---|---|
| `src/codegen/codegen-emit.cpp` | 1264 | separar emissão por área (params, expr, control flow) |
| `src/sema/hir-lower-expr.cpp` | 2357 | candidato secundário ainda acima de 1000 linhas |
| `src/frontend/frontend-expr.cpp` | 1115 | candidato secundário ainda acima de 1000 linhas |

Estado da quebra de `frontend-context.cpp` (concluída):

- `frontend-context.cpp`: entrada pública de parsing/frontend ou
  orchestration (315 linhas).
- `frontend-module-analysis.cpp`: análise e discovery de módulos
  (412 linhas).
- `frontend-module-cache.cpp`: bookkeeping do module cache (275 linhas).
- `frontend-source-catalog.cpp`: source catalog e helpers de fingerprinting
  (196 linhas).
- `frontend-symbol-resolution.cpp`: import requests e resolução de
  símbolos/módulos (764 linhas).

Estado da quebra de `compilation-session.cpp` (concluída):

- `compilation-session.cpp`: orquestração dos stages do pipeline e glue da
  sessão (871 linhas).
- `native-link.cpp`: helpers de native link/run (421 linhas).
- `persistent-cache.cpp`: helpers de cache persistente/object cache
  (721 linhas).
- `pipeline-plan.cpp`: contrato dos stages planeados (13 linhas).

Estado da quebra de `frontend.cpp`:

- `frontend.cpp`: snapshot/reconstruct/parse/canonical/functionSignature e
  orquestração pública (~199 linhas).
- `ast-lowerer.cpp`: lexer, CST builder, helpers do lowerer, `run()` e
  `skipMacroInvocation()` (~667 linhas).
- `frontend-types.cpp`: `parseType`, `isIntrinsicName` (~325 linhas).
- `frontend-expr.cpp`: call args, primary, postfix, expression/binary expression
  precedence e associativity (~1077 linhas).
- `frontend-stmt.cpp`: blocks, if/when, loops, condition, tag macro e statements
  (~1027 linhas).
- `frontend-decl.cpp`: imports, macros, implement, `lowerDeclaration`, campos de
  struct/interface e skip helpers (~1016 linhas).

O header `frontend/ast-lowerer.hpp` expõe `AstLowerer`, helpers de token e
`lex`/`parseCst`/`lowerAst`. `FrontendSnapshot` concede `friend` a
`lex`, `parseCst`, `lowerAst`, `AstLowerer` e `MacroExpander`.

Estado da quebra de `sema-modern.cpp`:

- `sema-modern.cpp`: construtor, `run()`, `prepareTypes()`, `checkExpressions()`,
  accessors, report helpers e `SemaPipeline` (182 linhas).
- `sema-decl.cpp`: registo de tipos, lowering de declarações, implement blocks,
  defaults de structs/functions.
- `sema-type.cpp`: lowering de tipos, foreign types, instanciação e resolução de
  declarações/interfaces.
- `sema-expr.cpp`: dispatcher de inferência e operadores unary/binary.
- `sema-call.cpp`: overload resolution, calls, variadic tail e default args.
- `sema-method.cpp`: métodos, `dyn`/traits/interfaces e constraints genéricas.
- `sema-control.cpp`: blocks, controlo de fluxo, defer, loops e returns.
- `sema-cast-coerce.cpp`: casts, coercions, narrowing e `opaque`.
- `sema-assign.cpp`: assignments, ownership, moves e raw reads.
- `sema-index.cpp`: index, field/arrow, enums e visibilidade.
- `sema-literal.cpp`: struct/array/pack literals, unions e defaults.
- `sema-state.cpp` / `sema-state-access.cpp`: state machines, dock/jump e
  resolução de nomes/accessors.
- `sema-zith.cpp`: const semantics, Zith-- checks e unificação.
- Helpers partilhados em `sema-modern-utils.{hpp,cpp}`.

Próxima fronteira:

- separar `codegen-emit.cpp` em áreas menores quando for prioridade.
- revisitar `hir-lower-expr.cpp` se continuar acima de ~1000 linhas após o
  split de codegen.
- revisitar `frontend-expr.cpp` apenas se continuar a ser um bottleneck claro
  de responsabilidade única.

Para o HIR lowering, a fronteira candidata foi já executada:

- `hir-lower-modern.hpp` continua a classe principal e o estado partilhado.
- `hir-lower-types.cpp`: `lowerType`, `lowerTypeExprConcrete`, `lowerForeignType`,
  `lowerTypeSize`, `lowerTypeAlign`, `lowerTagType`, `taggedMemberIndex`,
  `stableConcreteTypeId`.
- `hir-lower-expr.cpp`: `lowerExpr`, literals, nomes, unary/binary, field/arrow,
  index, slice, literal aggregates, casts, intrinsics, coercions e optional
  payloads.
- `hir-lower-call.cpp`: forms de call não-dyn e dyn, default args, variadic slice
  tail, `dyn` dispatch e tail calls.
- `hir-lower-block.cpp`: `lowerBlock`, `defer`, `if`, `when`, loops e condicoes.
- `hir-lower-stmt.cpp`: `lowerStatement`, bindings, return/break/continue e
  transições `state`/`jump`.
- `hir-lower-util.cpp`: helpers anónimos partilhados (`decodeEscapes`,
  `internFunctionKey`, `moduleNamespace`, `mapHirOwnership`,
  `mapHirEscape`).

O glob de `src/*.cpp` no CMake recolhe os novos ficheiros automaticamente; depois
de criar ficheiros, é preciso reconfigurar (`cmake -S . -B build`) para o glob
ver os novos `.cpp`.

---

## Dívida de duplicação e padrões repetitivos

### Duplicação de variadic tail logic (resolvida)

A decisão de `explicit_slice_arg` vs `auto_collect_tail` é produzida em sema
através de `VariadicCallPlan` e consumida mecanicamente pelo HIR lowering.
Os call sites que usavam regras locais para re-derivar a decisão foram
substituídos por leitura dos planos guardados em `TypedMap` para calls,
`dyn` calls, métodos e transições `state`/`jump`.

### Concatenação ProjectConfig + Options

Vários pontos de `src/session/compilation-session.cpp` repetem o padrão de
`dynArr.insert(end, mProjectConfig.X...)` + `mOpts.get().X...`:

- `includeDirs`: linhas ~353 e ~996.
- `cSourceDirs`: linha ~958.
- `defines`: linhas ~358 e ~1011.
- `libraryDirs` e `libraries`: linhas ~1142 e ~1151.

Acção recomendada: helper única `mergeStrings(config, options, field, append)`
para evitar erros de ordem e duplicação.

### Erro de instabilidade de tags `opaque`

O `E2010` para tags canónicas instáveis é emitido numa única mensagem em
[compilation-session.cpp](/home/diogo/Zith/src/session/compilation-session.cpp:503)
durante a hydration do cache. A mensagem pede ao utilizador para invalidar o
cache quando a canonização divergir.

Risco residual: existem vários ramos que criam/validam tags `opaque` e a
consistência entre a canonização nova e a persistida depende da mesma regra
usada no lowering em [hir-lower-expr.cpp](/home/diogo/Zith/src/sema/hir-lower-expr.cpp:786).
Uma mudança da canonical field order deve atualizar o registry/cache em conjunto.

### Split inicial por script deixou includes colados e métodos órfãos

Estado resolvido: includes colados foram partidos, `isOpaquePointerCast` foi
restaurado em `sema-cast-coerce.cpp`, e os helpers de interface/resolução de
`sema-modern.cpp` foram movidos para `sema-type.cpp`.

Risco residual: o split foi mecânico; a estrutura dos ficheiros é razoável, mas
a localização de cada método deve ser revista quando se mexer na área para
confirmar que está no ficheiro com a responsabilidade certa.

### HIR nodes sem initializers completos

Estado resolvido: todos os `HirExpr` alternatives em `src/hir/hir-expr.hpp` têm
default member initializers para ids, tipos, flags e scalar values; os nodes com
`memory::DynArray` continuam a depender dos construtores dedicados que recebem a
arena. O padrão mantém a construção agregada usada pelos lowers, sem exceções
ou RTTI.

---

## Próximos passos para rever

1. A quebra de HIR, de `sema-modern.cpp` e de `frontend.cpp` está feita; a
   quebra de `frontend-context.cpp` e `compilation-session.cpp` também está
   feita; a próxima prioridade é `codegen-emit.cpp`, com
   `hir-lower-expr.cpp` como candidato secundário.
   O contrato de execução para estes splits está em `docs/plans/monolith-splits.md`.
2. Em cada extracção, compilar `zithcLib` e correr os testes da área afectada;
   `ctest --test-dir build --output-on-failure` para regressões gerais.
3. Casos de incompletude que precisam de decisão de produto (sintaxe de `type`,
   slices literais, `is <type>`) devem ser tratados como issues separados, não
   como parte da quebra mecânica.
4. Consolidar as entradas duplicadas de `Known Debt` de `docs/impl-status.md`
   para este ficheiro quando forem tratadas.
