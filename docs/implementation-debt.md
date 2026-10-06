# Zith Implementation Debt

> Last updated: 2026-09-26 (release, CI and distribution contract audit).

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
- Os smoke tests dos installers usam agora `examples/loops-simple.zith`, que
  exercita o compilador sem depender de libclang/C-header interop. A presença
  de `std/io/console.zith` continua a ser validada separadamente para cobrir
  descoberta e empacotamento da stdlib.
- O manifest `.github/scoop/bucket/zithc.json` foi regenerado para a release
  publicada `v0.6.3.2` do repositório canónico `GalaxyHaze/Zith-Lang`: os dois
  binários Windows e o ZIP da stdlib têm agora os quatro SHA-256 verificados
  contra os digests da release. O job `update-distribution` de
  `build-artifact.yml` repete esse cálculo automaticamente para cada release e
  valida o manifest em modo estrito. `update-package.yml` ficou apenas como
  reparação manual para releases antigas.
- Os shims Scoop executam `zithc` a partir de `~\scoop\shims`, fora do prefixo
  da app. O binário Windows resolve o caminho do módulo real com
  `GetModuleFileNameW`, e o smoke Scoop agora executa um `check` com
  `std/io/console` através do shim. `ZITH_STDLIB` e `--include` continuam
  disponíveis como overrides explícitos.
- O novo layout de `scripts/install.ps1` (`%LOCALAPPDATA%\Zith\bin` +
  `%LOCALAPPDATA%\Zith\share\zith\stdlib`) e o ramo MSYS/MinGW/Cygwin de
  `scripts/install.sh` têm agora smoke tests no `build-artifact`. Os installers
  também aceitam `ZITH_INSTALL_ROOT` e `ZITH_RELEASE_BASE_URL`, permitindo testar
  uma instalação isolada sem privilégios administrativos. O teste local Unix
  confirmou a descoberta automática da stdlib a partir do layout instalado.
  O repositório padrão dos três installers é agora
  `GalaxyHaze/Zith-Lang`, que é o destino canónico do rename; forks podem
  definir `ZITH_REPOSITORY` sem editar os scripts.
  Continua pendente a primeira execução real da matriz Windows, incluindo ARM.
- Em `.github/workflows/build-artifact.yml` os targets ARM64 usam runners
  nativos e pedem LLVM explicitamente; CMake falha se LLVM 18+ não estiver
  disponível, em vez de publicar um compilador sem codegen. Os quatro jobs
  Windows fornecem também o `LLVM_DIR` do pacote Chocolatey, porque o layout
  CMake do LLVM não pode depender apenas do `PATH` do runner.
- O CI regular (`ci.yml`) exige agora LLVM 18+ nos builds nativos Debug/Release.
  O `build-artifact` adicionou smoke tests de `install.sh`, `install.ps1` e
  Scoop, além da validação estrutural do manifest. O CI WASM continua a
  executar o runtime ABI. A cobertura de runners ARM e a instalação Scoop
  permanecem dependentes de uma execução de release real.
- O workflow `create-new-release.yml` publica os comandos de instalação a partir
  de `raw.githubusercontent.com/${{ github.repository }}/main/...`, alinhado
  com a branch atual. O mesmo workflow agora expõe a versão normalizada
  (`v` removido do input) como output do job de tag e usa esse output ao chamar
  `build-artifact`, evitando tags inválidas como `vv1.2.3` quando o operador
  fornece a versão com prefixo `v`.
- O bucket Scoop e a fórmula Homebrew são actualizados no job
  `update-distribution` de `build-artifact.yml`; ambos dependem de
  `RELEASE_PAT` e do tap externo. O Homebrew recebe a fórmula completa através
  da Contents API, protegida pelo SHA do ficheiro remoto, em vez de depender de
  um workflow remoto que só substitua texto parcial. O smoke de `scoop install`
  corre depois da atualização automática do manifest, usando o conteúdo de
  `main`. Os contratos estáticos de Scoop/Homebrew usam o identificador
  canónico `GalaxyHaze/Zith-Lang`. Os workflows usam `github.repository`, que o
  GitHub resolve para esse nome após o rename de `GalaxyHaze/Zith`.
- A fórmula Homebrew passa explicitamente o diretório CMake de
  `Formula["llvm"].opt_lib`, pois LLVM é keg-only e não deve ser descoberto por
  acaso através do `PATH`. A fórmula exige LLVM e essa dependência é agora
  verificável pelo contrato offline.
- O job `validate-release-assets` bloqueia a atualização de distribuição até
  todos os binários nativos, variantes musl, LSP, WASM e os dois formatos da
  stdlib existirem na mesma release. Assim, uma release parcial não pode
  produzir metadados Scoop/Homebrew aparentemente válidos nem sincronizar uma
  versão incompleta para o playground.
- A validação de assets deixou de ser o único gate de publicação: `update-distribution`
  e `sync-playground-wasm` esperam também os smoke tests dos installers Unix,
  Windows e WASM. O smoke Scoop continua depois do update do manifest, porque
  depende precisamente dos hashes/URLs que esse job gera.
- `create-new-release.yml` mantém a release como draft durante toda a construção.
  O job `publish-release` só remove o draft depois de assets, installers,
  metadados de distribuição, Scoop e sincronização do playground WASM passarem.
  Uma falha intermédia deixa uma release draft diagnosticável em vez de expor
  uma release incompleta.
- `scripts/validate-release-contract.py` verifica cada job nativo
  (`build-main`, `build-musl` e `build-lsp`) individualmente, exigindo
  `ZITH_HAS_LLVM=ON` e `ZITH_REQUIRE_LLVM=ON`, e confirma que o job WASM fica
  fora desse requisito. O SHA do archive Homebrew é calculado a partir de um
  download local com `curl --fail --retry`, não de uma pipeline que possa
  mascarar uma resposta truncada.
- `scripts/test-release-contract.py` executa os dois geradores contra assets
  temporários e confirma os quatro hashes Scoop, a versão/URL/SHA Homebrew e a
  validação estrita do manifest. O CI corre este teste sem acesso à rede.
- `scripts/verify-llvm-build.py` lê o `CMakeCache.txt` gerado e impede que um
  job publique um artefacto sem o backend LLVM efetivamente configurado. A
  versão detectada é persistida por CMake e precisa de ser 18 ou superior.
- Os builds musl deixaram de resolver a versão mais recente do Zig em tempo de
  execução. O workflow fixa Zig `0.13.0`, confirma que o executável descarregado
  reporta essa versão e verifica no `CMakeCache.txt` que o triple configurado
  coincide com cada target musl da matriz. O teste offline cobre tanto a
  aceitação do target correto como a rejeição de um target diferente.
- O gate `validate-release-assets` também abre `zithc-wasm.zip` e exige
  `zith-playground.wasm` e `zith-stdlib.pack` não vazios antes de atualizar
  Scoop, Homebrew ou o playground. Essa regra vive agora em
  `scripts/validate-release-assets.py` e é exercitada pelo teste offline do
  contrato, evitando que a lista de assets do workflow e a validação do bundle
  WASM evoluam separadamente.
- `scripts/install-wasm.sh` aceita `ZITH_RELEASE_BASE_URL`, limpa o diretório
  anterior durante upgrades e falha se o bundle não trouxer o WASM ou o pack
  stdlib. O `build-artifact` executa agora um smoke test desse instalador.
- `update-package.yml` continua disponível apenas para reparar releases antigas,
  mas agora reutiliza `update-scoop-manifest.py` e
  `update-homebrew-formula.py`, com o mesmo cálculo fail-fast de SHA do fluxo
  automático. Os dois manifests são commitados juntos, e o workflow partilha
  com `build-artifact` um grupo de concorrência que serializa os pushes para
  `main`.
- `install.sh` e `install.ps1` fazem download e validação da stdlib em staging
  antes de substituir o binário ou a stdlib anterior. O caminho Unix foi
  testado para sucesso e falha sem deixar uma instalação parcial; a validação
  sintática/execução PowerShell permanece coberta pelo runner Windows, pois
  `pwsh` não está instalado no ambiente local.

---

## Não-dívidas (decisões de design)

| Item | Decisão |
|---|---|
| `const fn` | Não é pretendido em `Zith--`. O parser aceita `FunctionKind::Const`, mas o pipeline rejeita `const fn` com `UnsupportedSyntax` em [frontend-decl.cpp](/home/diogo/Zith/src/frontend/frontend-decl.cpp:406). Não documentar como dívida. |
| `dyn Interface` sem acesso a fields | O design expõe apenas métodos em `dyn`; fields ficam disponíveis em tipos concretos e bounds genéricos. `a.x on dyn Interface` com `E3001` é comportamento pretendido, não debt. |
| `type Name = T` cast-based | Em `Zith--`, `type Name = T` é nominal e o contrato explícito usa casts: `T as Name` constrói e `Name as T` extrai o campo subjacente. `Name` não é intercambiável com `T`; `alias Name = T` continua transparente. Sintaxe dedicada de construção/acesso é follow-up opcional, não é dívida activa. |
| Casts de utilizador | Não são pretendidos em `Zith--`. As conversões explícitas são os casts do spec (`as`, `raw as`), a coerção `opaque`, o narrowing `is` e os casts numéricos/ponteiro. Não existe trait de conversão (tipo `From`/`Into`/`Castable`) no spec nem na stdlib. Um novo branch de conversão em [classifyCast](/home/diogo/Zith/src/sema/sema-modern-utils.cpp:45) só seria adicionado se o spec de linguagem definir uma superfície dedicada. Não documentar como dívida. |
| `..` / `...` como um token `Dots` | O lexer produz um único `TokenKind::Dots` para uma corrida de pontos ([ast-lowerer.cpp](/home/diogo/Zith/src/frontend/ast-lowerer.cpp:226)) e o parser distingue a grafia pelo lexema: `..` é o range ([frontend-expr-operator.cpp](/home/diogo/Zith/src/frontend/frontend-expr-operator.cpp:101)) e `...` é o marcador de variadic slice `[...]T` ([frontend-types.cpp](/home/diogo/Zith/src/frontend/frontend-types.cpp:115)). É um detalhe do lexer, não um defeito de comportamento. |

---

## Dívidas reais (features implementadas mas incompletas e código com risco)

### 2. Cache `.zirl` (resolvida)

Resolved: o cache persiste e lê artefactos `.zirl` em
`src/cache/cache.cpp` (`Store::store` escreve com `zirl::Writer`; load/hydration
usam `zirl::Reader`), e o registry `canonical-any` é serializado/validado.
`tests/test-cache.cpp` cobre round-trips, hydration e divergência canónica.
O ficheiro `impl-status.md` foi atualizado de `Cache | Partial` para
`Cache | Working`.

### 3. NRA está parcial (reclassificado: é SRA, congelado por design)

- Estado atual: o que o Zith-- implementa é o SRA (Small Resource Analysis),
  uma fatia deliberadamente pequena e congelada: facts residuais e call
  annotations consumidos antes do lowering final, e a fatia de
  use-after-move de `&local`/`@ptrOf(local)` publicada como
  `HirConsumedState::Consumed` sem nodes de move no HIR.
- Reclassificação: o que aqui estava descrito como "faltas reais" (a máquina
  de estados completa e a prova de regras) não é dívida de Zith--. É trabalho
  de full Zith, agora especificado do zero em
  [nra-spec.md](/home/diogo/Zith/docs/nra-spec.md) com o novo modelo de
  estados (`uninitialized`/`taken`/`ok`, flow, edge state) e as regras
  NRA-1..NRA-11. Zith-- fica no SRA e não vai crescer para a prova completa.
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
- `is <type>` narrowing funciona para tagged unions e `opaque`; outros tipos
  ainda não são suportados.
- Narrowing após `is null` / `not (is null)` para aggregate optionals (`?T`
  com payload não-pointer) extrai o campo 0 no then/else correto. Para
  pointers (`?*T -> *T`) o mesmo controlo de fluxo prova non-null e usa deref,
  arrow, index e coerção; uso sem prova reporta `E3005` e `raw`/`must` são os
  opt-outs explícitos.
- Casts `int -> int` estreitantes verificam em compile time valores inteiros
  constantes reconhecidos pelo sema. A adaptação de literais numéricos não
  verifica a faixa; variáveis e conversões `float -> int` também não têm
  checks de overflow em runtime.
- Formatter reimprime `for (cond)` como `while` (`ExprKind::While` no
  round-trip).
- `realloc` no runtime VM v2/WASM foi resolvido: o allocator separa blocos
  alocados de blocos livres, preserva dados ao crescer/mover, suporta shrink e
  trata o offset zero como endereço válido. A cobertura está em
  `test-vm-v2` e no harness WASM.

Estas entradas detalham o estado real e as referências de bloqueio. A secção
`Known Debt` de [impl-status.md](/home/diogo/Zith/docs/impl-status.md) foi
consolidada neste ficheiro; as entradas duplicadas foram removidas de
`impl-status.md`. As dívidas partilhadas restantes são listadas abaixo com
nota de estado:

- Narrowing permanece incompleto: casts `int -> int` verificam valores
  constantes reconhecidos em compile time, mas a adaptação de literais não
  verifica faixa e casts com operandos variáveis ou `float -> int` não têm
  checks de overflow em runtime.
- Unchecked nullable-pointer coercion foi removida: a prova flow-sensitive
  após `is null` existe, e usos sem prova reportam `E3005`.
- `is <type>` narrowing beyond tagged unions and `opaque`.
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

### 10. Conformance importada é determinística; trait defaults ainda usam scans globais

Estado atual: a causa raiz da instabilidade foi removida. A resolução de
declarações, tipos e métodos importados passou a ser feita através das
imports/bindings visíveis do módulo atual (`Import` e `ModuleAlias`) em vez de
percorrer todos os módulos carregados. `tests/test-interface-satisfaction.cpp`
agora cobre qualified calls sobre `InPlace` importado de `std/memory` num workdir
populado com vários módulos não relacionados.

Dívida residual de escopo: trait defaults e requisitos de `dyn Trait` ainda
são procurados em todos os módulos carregados quando o trait não está no
módulo atual. Isso já não escolhe o trait errado para o caminho aqui
reproduzido, mas deve ser estreitado quando houver uma definição exata de quais
defaults estão visíveis a partir do módulo de chamada.

Ação futura: formalizar o alcance dos trait defaults e remover os restantes
scans globais. Esta dívida é independente da resolução determinística de
conformance importada já coberta pelos testes.

---

### 11. `export` de facades com prefixo partilhado

Estado atual: `export` re-injecta os símbolos públicos de cada alvo e
representa cada prefixo de namespace exportado como um `ModuleAlias`
independente com o mesmo nome de raiz. O caso de uso real é
`stdlib/std/memory.zith`, que reexporta `in-place`, `allocator`, `heap` e
`new` a partir de `from std/memory`.

O consumidor com `import std/memory` mantém aliases para cada subcaminho
exportado; `lookupModuleAliasForPath` escolhe o alias mais longo cujo
`modulePath` é prefixo do caminho qualificado. Assim
`std.memory.in-place.InPlace` e `std.memory.allocators.heap.HeapAllocator`
resolvem para os módulos reais de cada export. O teste de frontend e o
lowering até HIR cobrem o fanout em estados limpos e cached.

Ficou resolvida a dívida original de fanout com prefixo partilhado. A revisão
CLI encontrou dois limites adicionais no mesmo caminho de `export path`, agora
cobertos pelos testes de pipeline em estados limpos e cached.

### 11a. Tipos qualificados via facade `std/memory`

O consumidor pode importar `std/memory` e referenciar tipos pelo caminho
qualificado da facade:

```zith
import std/memory

fn main(): i32 {
    let a: ?std.memory.allocators.heap.HeapAllocator = null;
    if (a is null) {
        return 0;
    }
    return 1;
}
```

O caminho direto para o módulo folha continua válido:

```zith
import std/memory/allocators/heap
let a: ?std.memory.allocators.heap.HeapAllocator = null;
```

O bloqueio era anterior ao sema: `stdlib/std/memory.zith` partilha o nome
lógico `std/memory` com o diretório `stdlib/std/memory/`. O resolver agregava
primeiro o diretório e escolhia `in-place` como alvo, em vez de carregar a
facade e seguir o alias mais longo para `heap`. Agora ficheiros regulares,
incluindo `memory.zith`, têm precedência sobre diretórios homónimos. O lowering
consulta a declaração pública do tipo no módulo folha. O teste cobre
`HeapAllocator` e `InPlace` em execução fria e após hidratação do cache.

### 11b. Segmentos kebab-case em tipos qualificados

O parser mantém segmentos kebab-case em tipos qualificados:

```zith
import std/memory
let a: ?std.memory.in-place.InPlace = null;
```

O parser junta tokens de hífen contíguos dentro do segmento e preserva os
limites indicados pelos pontos. O teste de frontend verifica os segmentos e o
teste de pipeline verifica `std.memory.in-place.InPlace` via facade.

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

O job [update-distribution](/home/diogo/Zith/.github/workflows/build-artifact.yml:309)
resolve a tag, calcula os hashes dos assets Windows/stdlib e calcula o sha256
do archive GitHub em
`https://github.com/GalaxyHaze/Zith-Lang/archive/refs/tags/v${version}.tar.gz`, depois
faz `PUT` da fórmula completa em `GalaxyHaze/homebrew-zithc` através da Contents
API. A fórmula recomendada fica documentada em
[zithc.rb](/home/diogo/Zith/.github/homebrew/zithc.rb): build a partir do source
tag com `-DZITH_HAS_LLVM=ON -DZITH_REQUIRE_LLVM=ON`, e stdlib instalada em
`share/zith/stdlib`, o caminho já lido por
[stdlib-discovery.cpp](/home/diogo/Zith/src/support/stdlib-discovery.cpp:107).

O archive e o SHA da release `v0.6.3.2` foram verificados localmente. O
workflow futuro recalcula esse SHA a partir do archive da tag e envia os mesmos
dados no dispatch.

Risco residual e validação em aberto:

- A fórmula atualmente publicada ainda não exige LLVM nem instala a stdlib no
  layout esperado. O `build-artifact` agora substitui a fórmula inteira na
  release seguinte, mas a convergência inicial e o build Homebrew real do tap
  continuam pendentes até uma execução autenticada do fluxo.
- O release atual não publica um binário macOS estável usado pela fórmula. A
  escolha defensável é build-from-source do archive de tag, sem inventar hash.
- A atualização usa `github.repository_owner`, portanto o tap é `GalaxyHaze`
  quando este repo o usar como remote. O workflow remoto antigo pode continuar
  no tap para compatibilidade, mas já não é a fonte de sincronização.

Acção recomendada: atualizar o tap externo para a fórmula LLVM-enabled desta
rama e confirmar o build Homebrew numa release real. Essa alteração fica fora
da work-tree deste repositório.

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
