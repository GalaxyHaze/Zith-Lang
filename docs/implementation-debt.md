# Zith Implementation Debt

> Last updated: 2026-09-17 (cache/ZIRL, monolith split and IR VM curation).

Documento de gestão da dívida de implementação. Distingue propositadamente:

- **Dívidas reais**: funcionalidade que foi implementada de forma incompleta, com
  limitação conhecida que não é a intenção de design, ou código que repete lógica
  e dificulta manutenção.
- **Não-dívidas**: decisões de design intencionais para `Zith--`. Manter o
  comportamento atual, mesmo que pareça incompleto comparado com o spec maior do
  Zith.

Este ficheiro não substitui `docs/impl-status.md`. É o inventário de trabalho de
engenharia para rever e gerir.

---

## Release stdlib / installer follow-ups

- Os installers de release substituem o conteúdo de `<prefix>/share/zith/stdlib`
  antes de extrair, para evitar ficheiros stdlib antigos após um upgrade.
- O manifest `.github/scoop/bucket/zithc.json` aponta para
  `GalaxyHaze/Zith` versão `0.6.3`, mas os quatro hashes ficaram como
  placeholders vazios. O workflow `update-package.yml` já usa
  `${{ github.repository }}` e deve regenerar URLs/hashes no próximo release.
  Hashes não foram inventados nesta rama porque a auditoria correu sem acesso
  a artefactos publicados.
- Os shims Scoop executam `zithc` a partir de `~\scoop\shims`, fora do prefixo
  da app. A descoberta automática por caminho do exe só é garantida para
  invocação direta do binário real. O workaround documentado é `ZITH_STDLIB`
  ou `--include`.
- O novo layout de `scripts/install.ps1` (`%LOCALAPPDATA%\Zith\bin` +
  `%LOCALAPPDATA%\Zith\share\zith\stdlib`) e o novo ramo MSYS/MinGW/Cygwin de
  `scripts/install.sh` dependem do runtime ser testado em Windows real; até
  esse teste estar em CI, ficam verificados por sintaxe e pela análise estática
  do código.
- Em `.github/workflows/build-artifact.yml` o job Windows ARM64 usa
  `msvc_arch: amd64_arm64`, pelo que o step "Setup Zig (for Windows arm64
  cross-compile)" está morto (`if: ... msvc_arch == ''`). O target atual usa
  clang-cl/LLVM para ARM64. O dead step deve ser removido ou a estratégia deve
  ser resolvida antes de confiar num segundo fallback Zig.
- O CI regular (`ci.yml`) só corre nativo em `ubuntu-latest`. Não valida os
  installers `install.ps1`/`install.sh`, o layout Scoop, os artifacts de release
  contra `findStdlibRoots()`, nem executa um smoke test de stdlib após instalar.
  A validação efetiva desses caminhos fica residualmente sem cobertura
  automática até haver um job Windows/macOS ou um teste de instalação em
  diretório temporário.
- O workflow `create-new-release.yml` aponta para
  `raw.githubusercontent.com/${{ github.repository }}/master/...`, mas a branch
  atual do repositório é `main`. Os comandos de instalação publicados num
  release podem apontar para uma branch inexistente/antiga até o script ser
  corrigido para `main`.
- O bucket Scoop e o dispatch Homebrew são actualizados por
  `update-package.yml`. As falhas desse fluxo não são visíveis em PRs deste
  repo e dependem de `RELEASE_PAT`/do tap externo. A regeneração de hashes e o
  teste de `scoop install` só podem ser confirmados fora desta rama ou num
  follow-up manual.

---

## Não-dívidas (decisões de design)

| Item | Decisão |
|---|---|
| `const fn` | Não é pretendido em `Zith--`. O parser aceita `FunctionKind::Const`, mas o pipeline rejeita `const fn` com `UnsupportedSyntax` em [frontend-decl.cpp](/home/diogo/Zith/src/frontend/frontend-decl.cpp:406). Não documentar como dívida. |
| `dyn Interface` sem acesso a fields | O design expõe apenas métodos em `dyn`; fields ficam disponíveis em tipos concretos e bounds genéricos. `a.x on dyn Interface` com `E3001` é comportamento pretendido, não debt. |
| `type Name = T` cast-based | Em `Zith--`, `type Name = T` é nominal e o contrato explícito usa casts: `T as Name` constrói e `Name as T` extrai o campo subjacente. `Name` não é intercambiável com `T`; `alias Name = T` continua transparente. Sintaxe dedicada de construção/acesso é follow-up opcional, não é dívida activa. |

---

## Dívidas reais (features implementadas mas incompletas e código com risco)

### 2. Cache `.zirl` (resolvida)

Resolved: o cache persiste e lê artefactos `.zirl` em
`src/cache/cache.cpp` (`Store::store` escreve com `zirl::Writer`; load/hydration
usam `zirl::Reader`), e o registry `canonical-any` é serializado/validado.
`tests/test-cache.cpp` cobre round-trips, hydration e divergência canónica.
O ficheiro `impl-status.md` foi atualizado de `Cache | Partial` para
`Cache | Working`.

### 3. NRA está parcial

- Estado atual: facts residuais e call annotations existem e são consumidos antes
  do lowering final. A fatia de use-after-move está resolvida: `&local` e
  `@ptrOf(local)` marcam o slot como `knownAlive = false`, o sema continua a
  reportar `E4001` por leituras posteriores e o lowering publica o slot como
  `HirConsumedState::Consumed` sem nodes de move no HIR.
- Faltas reais: o state machine completo alive/dead/lent e a prova de quatro
  regras não existem; não há todos os diagnósticos de ownership previstos.
  Moves de receivers por valor por chamadas/métodos e a propagação de
  obsolescência entre branches ainda dependem da máquina completa.
- Referência: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:41).

### 4. Bare `opaque` usa hydration estável mas ainda depende de canonização consistente

- Estado atual: o typeId canónico é derivado do namespace do módulo, ordem
  canónica de fields e nome do tipo. O registry é o contract explícito de
  `canonical-any`: cada canonical id project-local recebe um runtime tag único
  numa cold build, o mapping é persistido e os artefactos serializam a mesma
  tabela em `canonical_mappings`. Na hydration warm, a tabela é validada contra
  o registry antes de re-hidratar `TypeIntern`; quando diverge, o `E2010`
  identifica a canonical id exacta e recomenda apagar `canonical-any` e os
  artefactos `.zirl`, depois reconstruir.
- Regra de evolução escolhida: mudanças de canonical field order alteram o
  canonical id e por isso não são re-mapeadas automaticamente. Uma build com
  canonização nova continua determinística, mas qualquer artefacto com um tag
  persistido antigo é rejeitado com o caminho de recuperação acima em vez de
  ser silenciosamente re-tagado. Sem mudança de canonical id, cold build,
  hydration warm, imports e valores `opaque` reutilizam o mesmo runtime tag.
- Dívida real: continua sem existir um registry object no runtime do programa;
  o contract é project-local no compiler/cache. A detecção de divergência
  distingue um tag antigo desconhecido de um tag reutilizado por outro
  canonical id, mas ainda não categoriza qual field concretamente mudou.
- `coerceValue` trata `opaque -> opaque` como no-op de sema, pelo que casts
  vindos de módulos importados não são rejeitados como erro de re-tagging.
- Referência: hydration e erro de instabilidade em
  [sema-cast-coerce.cpp](/home/diogo/Zith/src/sema/sema-cast-coerce.cpp),
  hydration em
  [persistent-cache.cpp](/home/diogo/Zith/src/session/persistent-cache.cpp:34)
  e testes em
  [test-cache.cpp](/home/diogo/Zith/tests/test-cache.cpp:347).

### 5. C interop é `Working (validated C)`, não ABI completa

- Estado atual: libclang cobre C comum, variadics, parâmetros array-decayed,
  `va_list` e function pointers. Object-like scalar macros são importadas como
  constantes.
- Dívida real: struct-by-value ABI é limitado a records simples cuja
  layout/alignment libclang prova para o target configurado; scalars, pointers
  e nested records verificados são suportados; além do slot i64 (scalar
  64-bit/pointer ou dois i32 adjacentes), um record com dois campos 64-bit
  adjacentes também é validado por valor como dois registos 64-bit, que é o
  shape que Clang usa no x86-64 e AArch64 Linux. Bitfields, packed/anonymous
  records, flexible arrays, mixed-width records, globals, strings,
  function-like macros, `long double` e `__int128` não são importados.
- Referência: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:153).

### 6. Outras incompletudes registadas
- Literal range forms `1..5`, `1>..5`, `1..<5` e `1>..<5` baixam para
  `ExprKind::Range` com bounds brutos e flags `openAtLo`/`openAtHi`; as quatro
  grafias são provadas em `test-codegen.cpp`, `test-frontend.cpp`,
  `test-formatter.cpp` e agora com ranges vazios e de fronteira única em
  `tests/test-optional-slice.cpp`. `1..5` é `[lo, hi]`, `1>..5` é `(lo, hi]`,
  `1..<5` é `[lo, hi)`, `1>..<5` é `(lo, hi)`; ranges reversed/empty não
  iteram e `in` devolve falso para `[x, x)`/`(x, x]`/`(x, x)` e verdadeiro
  para `[x, x]`. Um `for` literal-range sem binding é rejeitado em sema com
  diagnóstico direcionado antes de HIR/codegen.
- `is <type>` fora de unions/opaque não existe.
- Narrowing após `is null` / `not (is null)` para aggregate optionals (`?T`
  com payload não-pointer) extrai o campo 0 no then/else correto. Para
  pointers (`?*T -> *T`) o mesmo controlo de fluxo prova non-null e usa deref,
  arrow, index e coerção; uso sem prova reporta `E3005` e `raw`/`must` são os
  opt-outs explícitos.
- Casts numéricos estreitantes não verificam overflow.
- `++` / `--` não existem.
- Formatter reimprime `for (cond)` como `while` (`ExprKind::While` no
  round-trip).
- `..` é lexado caractere a caractere.

Estas entradas detalham o estado real e as referências de bloqueio. A secção
`Known Debt` de [impl-status.md](/home/diogo/Zith/docs/impl-status.md) foi
consolidada neste ficheiro; as entradas duplicadas foram removidas de
`impl-status.md`. As dívidas partilhadas restantes são listadas abaixo com
nota de estado:

- No overflow check on narrowing conversions.
- Unchecked nullable-pointer coercion foi removida: a prova flow-sensitive
  após `is null` existe, e usos sem prova reportam `E3005`.
- `is` outside `null`/tagged-union contexts.
- User-defined casts (novo branch em `classifyCast`).
- C struct-by-value ABI limited to verified simple records.
- Imported/cached bare `opaque` values: registry project-local, sem registry
  object em runtime e sem categorização do field que mudou.
- Ownership proof still happens after premature lowering in places.
- Expression statements whose root is a non-void call require explicit
  `_ = expr;` (`E2026`); the existing Zith-- docs now call this default
  behavior, not an attribute.

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
- Não-dívida: `*char` fica intencionalmente fora do contrato actual. Strings
  continuam disponíveis pelo adapter `text()` do `InputLine`. Adicionar
  parsing de outros primitivos é uma extensão opcional aos mesmos
  `implement ... as ParseInput`, não uma lacuna do contrato actual.
- Referência de estado: [impl-status.md](/home/diogo/Zith/docs/impl-status.md:45)
  na linha `Stdlib I/O` e o plano completo em
  [parse-input-cast.old.md](/home/diogo/Zith/docs/plans/archive/parse-input-cast.old.md).

### 9. Structs genéricas de primeira classe para stdlib (implementado)

Resolution: **implemented** (0.7.0 generics work).

O `HashMap<K, V>` genérico deixou de estar bloqueado: a reificação guarda os
argumentos concretos como metadados estruturais no `StructType`, usa
`GenericInstantiationPass::substituteType` como helper central de reificação,
propaga bounds genéricos através de fields e chamadas, e persiste os type args
no cache. `stdlib/std/collections/hash_map.zith` passa `zithc check` sem
`E2006`, `E3001`, `E3003`, `E3011` ou `E3009`. A superfície `u64 -> u64`
(`stdlib/std/collections/hash_map_u64.zith`) permanece inalterada.

Verificação registada em 2026-09-11:
- `./build/zithc --include stdlib check stdlib/std/collections/hash_map.zith`
  passa.
- `constraint-propagation.zith`, `nested-struct.zith`, `test-pair.zith` e
  `return-optional.zith` passam como probes.
- `tests/test-generic-hashmap.cpp` está registado com `add_zith_test` e passa.
- CTest focado em generics/hashmap/cache passa.
- O plano e o ADR de identidade estão em
  [generic-hashmap-debt-09.md](/home/diogo/Zith/docs/plans/0.7.0/generic-hashmap-debt-09.md)
  e [0019-concrete-generic-type-identity.md](/home/diogo/Zith/docs/adr/0019-concrete-generic-type-identity.md).

Dívida residual conhecida: o probe `entry-table.zith` que usa deref em cadeia
`self->table->occupied` não é o caminho do `hash_map.zith` real (que usa
`raw self->table[...]`) e continua fora deste item. A propagação de bounds via
`GenericBinding` é o contrato actual; `StructType.args` são metadados concretos,
não declarações de bound.

Ações de follow-up recomendadas:
- Promover os restantes probes de `/tmp/zith-generic-probes/` para
  `tests/test-generic-hashmap.cpp` quando o deref em cadeia de `?*T` for
  suportado.
- Fazer a passada de curadoria prevista para as declarações genéricas das
  stdlib: `string.zith`, `alloc.zith`, `DynArray` planned e o próprio
  `HashMap<K, V>`.

---

### 10. Traits/interfaces importadas têm conformance instável em workdirs populados

Estado atual: a causa raiz da instabilidade foi removida. A resolução de
declarações, tipos e métodos importados passou a ser feita através das
imports/bindings visíveis do módulo atual (`Import` e `ModuleAlias`) em vez de
percorrer todos os módulos carregados. `tests/test-interface-satisfaction.cpp`
agora cobre qualified calls sobre `InPlace` importado de `std/memory` num workdir
populado com vários módulos não relacionados.

Dívida real restante: trait defaults e requisitos de `dyn Trait` ainda são
procurados em todos os módulos carregados quando o trait não está no módulo
atual. Isso já não escolhe o trait errado para o caminho aqui reproduzido, mas
deve ser estreitado para resolver como os restantes padrões de método quando
houver uma definição exata de quais defaults estão visíveis a partir do módulo
de chamada.

Ação futura: quando o alcance de trait defaults for formalizado, repetir a
mesma passada e remover os restantes scans globais. O exemplo pode então migrar
de um trait local para o trait importado sem sacrificar a cobertura.

---

### 11. `export` de facades com prefixo partilhado

Estado atual: `export` re-injecta os símbolos públicos de cada alvo e mantém
apenas um alias de namespace por prefixo quando a mesma fachada reexporta mais
de um módulo com raiz comum (`export std/a` + `export std/b`). O caso de uso
real é `stdlib/std/memory.zith`, que reexporta `in-place`, `allocator`, `heap`
e `new` a partir de `from std/memory`.

Dívida real restante: um consumidor com `import std/memory` vê o namespace
qualificado a partir do primeiro export deduplicado, mas não deriva aliases
intermédios para todos os subcaminhos. Para caminhos totalmente qualificados
com fanout, o modelo de `ModuleAlias` precisa de representar namespaces como
nós, não apenas o primeiro segmento de cada import.

Ação futura: adicionar um mapa de namespaces por prefixo no `FrontendContext`
ou representar cada segmento de import como um alias independente, e cobrir
`std.memory.in-place.InPlace` e `std.memory.allocators.heap.HeapAllocator` com
testes de pipeline.

### 12. Receivers `dyn`/`lend` mutáveis para sinks

Estado atual: o contrato alvo de formatação é `TextSink`, com ligação de
destino por empréstimo dinâmico (`lend dyn TextSink`), mas o compilador ainda
bloqueia a escrita através desse caminho. `dyn TextSink` com um método mutável
`append(var self, ...)` compila, mas o data pointer aponta para um spill/cópia
em vez do valor original, pelo que as mutações não chegam ao caller. Receptores
`lend dyn TextSink` falham com `E3001`/`E2007`; `self: lend Self` e `self: lend
dyn TextSink` em traits também falham.

Por isso `stdlib/std/io/format.zith` usa `FormatBuffer` como sink real na
primeira versão. A intent `TextSink` fica registada em
`docs/adr/0022-stdlib-io-format-split.md`, `memory/stdlib-io-format.md` e
`CONTEXT.md`.

Ação futura: reparar `lend dyn` como receiver e fazer `emitMakeDyn` apontar
para o lvalue original quando a fonte é addressable, em vez de spillar valores
para uma nova `alloca`. Depois disso, migrar `Formatable.format(self, dest)` e
as helpers de append de `lend FormatBuffer` para `lend dyn TextSink`, cobrindo
também `[]char`/buffers fixos.

## Dívida de estrutura: monolitos

Os splits mecânicos de pipeline por ficheiro foram concluídos e estão fora da
lista activa:
`src/session/frontend-context.cpp`, `src/session/compilation-session.cpp`,
`src/codegen/codegen-emit.cpp`, `src/sema/hir-lower-expr.cpp` e
`src/frontend/frontend-expr.cpp`.

Estado da quebra de `codegen-emit.cpp` (concluída):

- `codegen-emit.cpp`: classe e orquestração (9 linhas).
- `codegen-emit-expr.cpp`: emissão de expressões (984 linhas).
- `codegen-emit-stmt.cpp`: emissão de statements/control flow (163 linhas).
- `codegen-emit-agg.cpp`: emissão de agregados (133 linhas).

`tests/test-codegen` corre com `396 passed, 0 failed` no estado atual.

Estado da quebra de `frontend-context.cpp` (concluída):

- `frontend-context.cpp`: entrada pública de parsing/frontend ou
  orchestration (315 linhas).
- `frontend-module-analysis.cpp`: análise e discovery de módulos
  (412 linhas).
- `frontend-module-cache.cpp`: bookkeeping do module cache (275 linhas).
- `frontend-source-catalog.cpp`: source catalog e helpers de fingerprinting
  (196 linhas).
- `frontend-symbol-resolution.cpp`: import requests e resolução de
  símbolos/módulos (777 linhas).

Estado da quebra de `compilation-session.cpp` (concluída):

- `compilation-session.cpp`: orquestração dos stages do pipeline e glue da
  sessão (879 linhas).
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
  precedence e associativity (~1077 linhas; atual split em baixo).
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

Estado da quebra de `frontend-expr.cpp` (concluída):

- `frontend-expr.cpp`: `parseCallArgument`, `parseExpression` e dispatcher
  (325 linhas).
- `frontend-expr-primary.cpp`: `parsePrimary`, `parsePostfix` e
  `parseAttributeValue` (742 linhas).
- `frontend-expr-operator.cpp`: helpers de operador, precedência,
  `functionKindPrefix` e predicates de ranges (141 linhas).

Estado da quebra de `hir-lower-expr.cpp` (concluída):

- `hir-lower-modern.hpp` continua a classe principal e o estado partilhado.
- `hir-lower-types.cpp`: `lowerType`, `lowerTypeExprConcrete`, `lowerForeignType`,
  `lowerTypeSize`, `lowerTypeAlign`, `lowerTagType`, `taggedMemberIndex`,
  `stableConcreteTypeId`.
- `hir-lower-expr.cpp`: dispatcher `lowerExpr` e glue pública (121 linhas).
- `hir-lower-expr-value.cpp`: value lowering, coercions, literals, nomes, casts,
  pipelines e optional payloads (1668 linhas).
- `hir-lower-expr-access.cpp`: `lowerLValueAddr`, `lowerIndex`, `lowerSliceRange`,
  `lowerField` e `lowerArrow` (379 linhas).
- `hir-lower-expr-agg.cpp`: `enumVariantValue`, `lowerStructLiteral`,
  `lowerPackLiteral`, `lowerArrayLiteral` e `lowerFieldDefault` (311 linhas).
- `hir-lower-call.cpp`: forms de call não-dyn e dyn, default args, variadic slice
  tail, `dyn` dispatch e tail calls.
- `hir-lower-block.cpp`: `lowerBlock`, `defer`, `if`, `when`, loops e condicoes.
- `hir-lower-stmt.cpp`: `lowerStatement`, bindings, return/break/continue e
  transições `state`/`jump`.
- `hir-lower-util.cpp`: helpers anónimos partilhados (`decodeEscapes`,
  `internFunctionKey`, `moduleNamespace`, `mapHirOwnership`,
  `mapHirEscape`).

O glob de `src/*.cpp` no CMake recolhe os novos ficheiros automaticamente. Depois
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

As concatenações de `ProjectConfig` + `Options` passaram a usar o helper
`session::mergeStrings` em `src/session/project-options-merge.hpp`. O helper
mantém a precedência atual de project config primeiro e CLI options segundo,
aceita um flag `append`, e suporta um appender opcional para call sites que
normalizam caminhos antes de inserir no destino. Os call sites cobertos são
`includeDirs`, `cSourceDirs`, `defines`, `libraryDirs` e `libraries` em
`src/session/compilation-session.cpp` e `src/session/native-link.cpp`.

### Erro de instabilidade de tags `opaque`

O `E2010` para tags canónicas instáveis é emitido durante a hydration do cache
em [persistent-cache.cpp](/home/diogo/Zith/src/session/persistent-cache.cpp:34).
A mensagem identifica a canonical id exacta e recomenda o comando
determinístico de apagar `canonical-any` e os artefactos `.zirl`, depois
reconstruir.

Risco residual: existem vários ramos que criam/validam tags `opaque` e a
consistência entre a canonização nova e a persistida depende da mesma regra
usada no lowering em [hir-lower-expr-value.cpp](/home/diogo/Zith/src/sema/hir-lower-expr-value.cpp:786).
Uma mudança da canonical field order deve atualizar o registry/cache em conjunto.

### Split inicial por script deixou includes colados e métodos órfãos

Estado resolvido: includes colados foram partidos, `isOpaquePointerCast` foi
restaurado em `sema-cast-coerce.cpp`, e os helpers de interface/resolução de
`sema-modern.cpp` foram movidos para `sema-type.cpp`.

Risco residual: o split foi mecânico. A estrutura dos ficheiros é razoável, mas
a localização de cada método deve ser revista quando se mexer na área para
confirmar que está no ficheiro com a responsabilidade certa.

### HIR nodes sem initializers completos

Estado resolvido: todos os `HirExpr` alternatives em `src/hir/hir-expr.hpp` têm
default member initializers para ids, tipos, flags e scalar values. Os nodes com
`memory::DynArray` continuam a depender dos construtores dedicados que recebem a
arena. O padrão mantém a construção agregada usada pelos lowers, sem exceções
ou RTTI.

### Contrato Homebrew depende de um tap externo não verificável localmente

O workflow [update-package.yml](/home/diogo/Zith/.github/workflows/update-package.yml:144)
resolve a tag e calcula o sha256 do archive GitHub em
`https://github.com/GalaxyHaze/Zith/archive/refs/tags/v${version}.tar.gz`, depois
dispara `repository_dispatch` para `GalaxyHaze/homebrew-zithc`. A fórmula
recomendada fica documentada em
[zithc.rb](/home/diogo/Zith/.github/homebrew/zithc.rb): build a partir do source
tag com `-DZITH_HAS_LLVM=OFF -DZITH_ENABLE_FFI=OFF`, e stdlib instalada em
`share/zith/stdlib`, o caminho já lido por
[stdlib-discovery.cpp](/home/diogo/Zith/src/support/stdlib-discovery.cpp:107).

Risco residual e validação em aberto:

- Não há fonte local para o conteúdo/fórmulas de `GalaxyHaze/homebrew-zithc`.
  Sem rede não é possível confirmar se o tap está desatualizado, apontando para
  um owner antigo (`GalaxyHaze/homebrew-zith`) ou se realmente possui a fórmula
  esperada.
- O release atual não publica um binário macOS estável usado pela fórmula. A
  escolha defensável é build-from-source do archive de tag, sem inventar hash.
- O dispatch usa `github.repository_owner`, portanto o tap é `GalaxyHaze`
  quando este repo o usar como remote. O local `Zith-Lang` nos manifests Scoop
  é uma divergência externa que deve ser corrigida no tap/release automation.

Acção recomendada: validar o tap com acesso de rede, verificar se a fórmula
aceita o payload `new-release` e substituir os placeholders de versão/sha256
quando houver um release tag real.

---

## Próximos passos para rever

1. A quebra de HIR, de `sema-modern.cpp`, de `frontend.cpp`, de
   `frontend-context.cpp`, de `compilation-session.cpp`, de `codegen-emit.cpp`,
   de `frontend-expr.cpp` e de `hir-lower-expr.cpp` está feita. Não há
   candidatos ativos na lista de monolitos. O contrato de execução para estes
   splits está em `docs/plans/monolith-splits.md`.
2. Em cada extracção, compilar `zithcLib` e correr os testes da área afectada.
   Use `ctest --test-dir build --output-on-failure` para regressões gerais.
3. Casos de incompletude que precisam de decisão de produto (sintaxe de `type`,
   slices literais, `is <type>`) devem ser tratados como issues separados, não
   como parte da quebra mecânica.
4. Remover entradas `Known Debt` de `docs/impl-status.md` apenas quando a
   dívida correspondente for realmente resolvida; neste passo a consolidação
   uniu as listas sem alterar o comportamento do compilador.
