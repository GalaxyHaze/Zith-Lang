# Plano: tornar `zithc docs` um gerador de API útil

## Objetivo

Transformar `zithc docs` num gerador determinístico de documentação Markdown do
entry point e dos módulos alcançáveis, com modos `interface` e `spec`, saída
segura em ficheiros e erros explícitos.

## Pré-condições

- `cwd` é `/home/diogo/Zith`.
- O build de desenvolvimento está configurado em `/home/diogo/Zith/build`.
- O utilizador autorizou o uso de Sequenta para acompanhar a implementação.
- A implementação atual de `docs()` está em
  `/home/diogo/Zith/src/cli/cmd/tool.cpp`. Analisa apenas
  `opts.inputFiles[0]`, percorre `snapshot->modules()` e imprime os
  `publicSymbols` como nomes e tipos.
- `Cli::dispatch()` ainda não injeta `config.projectRoot` para `Command::Docs`.
- As fontes da biblioteca são descobertas por `file(GLOB_RECURSE)` no
  `/home/diogo/Zith/CMakeLists.txt`. Depois de adicionar `.cpp`, voltar a
  configurar o build para atualizar o glob.
- Preservar todas as alterações existentes no worktree. Não editar nem
  reverter alterações alheias a este plano.
- Não executar `cmake --build build --target fmt`, porque pode reformatar
  ficheiros sem relação com esta tarefa. Usar `fmt-check`.
- Não executar `git reset --hard`, `git checkout --` ou comandos que apaguem
  ficheiros do utilizador.
- Não alterar `.sequenta/state.json` manualmente. Usar o CLI Sequenta.
- O parser Sequenta não aceita comentários livres depois do último bloco de
  tarefa em `context.sequenta`; manter notas de fase posterior no plano
  Markdown, não no contexto executável.
- Os IDs de teste Sequenta são únicos em todos os módulos do projeto; usar o
  prefixo `DOCS-` para os casos deste trabalho.
- A primeira entrega é Markdown. JSON é uma fase posterior e não deve bloquear
  nem ser parcialmente implementado nesta entrega.

## Decisões de produto

- O modo predefinido é `interface`.
- Aceitar `--interface`, `--spec` e `--mode=interface|spec`. Rejeitar
  combinações explícitas contraditórias, por exemplo `--spec --interface` ou
  `--mode=spec --interface`. Não alterar o significado global de
  `--mode <debug|dev|release|fast|small>` usado pelos outros comandos.
- Sem `--out`, escrever o documento Markdown agregado no terminal. O formato
  Markdown é o único formato implementado nesta fase.
- `--out` sem valor escolhe o diretório `docs/` do projeto, ou um diretório
  `docs/` junto ao ficheiro base quando a entrada é isolada.
- `--out=PATH` escolhe explicitamente o diretório de saída. O documento
  agregado nesse diretório chama-se `API.md`.
- O modo agregado é o comportamento predefinido. `--index` exige `--out` e
  gera `README.md` como índice e um ficheiro por módulo em `modules/`.
  Converter cada chave de módulo num nome de ficheiro seguro e estável.
  Detetar colisões antes de escrever e reportá-las como erro.
- `interface` inclui símbolos públicos do projeto e símbolos reexportados.
  Identificar reexports e indicar a origem do símbolo, com referência para a
  página do módulo de origem quando `--index` estiver ativo.
- `spec` inclui todos os símbolos dos módulos do projeto alcançáveis pelo
  entry point. De dependências, inclui apenas símbolos usados pelo projeto ou
  reexportados.
- O grafo começa no entry point configurado em `ZithProject.toml`, ou no
  ficheiro/diretório indicado explicitamente. Seguir imports e exports
  alcançáveis. Não documentar fontes órfãs apenas porque estão dentro de
  `src_dir`.
- Incluir assinaturas completas, tipos, parâmetros genéricos, campos,
  variantes, visibilidade, origem e comentários de documentação. Abranger
  funções e externs, macros, imports/exports, aliases, structs, enums, unions,
  traits, interfaces, context e state.
- Normalizar os comentários `/** ... */` e preservar o conteúdo como Markdown.
  O frontend já classifica documentação como `TriviaKind::DocLine` e
  `TriviaKind::DocBlock`; associar a trivia ao token inicial da declaração.
- Sem `--error`, qualquer erro de compilação ou geração impede a emissão de
  documentação parcial. Com `--error`, emitir a documentação válida e uma
  secção de erros agrupada por ficheiro, módulo e secção, e terminar com exit
  code `1`.
- Exigir `--force` antes de substituir qualquer destino existente. Nunca
  apagar ficheiros que não pertençam à saída desta execução. Ficheiros antigos
  que deixem de ser gerados não são removidos automaticamente.
- JSON terá schema versionado e representará a mesma informação semântica do
  Markdown. A interface acordada para essa fase é `--format=md|json`, com
  `--json` como atalho. Não implementar nesta primeira entrega.

## Ficheiros da implementação

- `/home/diogo/Zith/src/cli/options.hpp`
- `/home/diogo/Zith/src/cli/options.cpp`
- `/home/diogo/Zith/src/cli/cmd/tool.cpp`
- `/home/diogo/Zith/src/cli/cmd/info.cpp`
- `/home/diogo/Zith/src/cli/cmd/completion.cpp`
- `/home/diogo/Zith/src/cli/docs/doc-model.hpp` e `.cpp` (novos)
- `/home/diogo/Zith/src/cli/docs/markdown-renderer.hpp` e `.cpp` (novos)
- `/home/diogo/Zith/src/cli/docs/output-writer.hpp` e `.cpp` (novos)
- `/home/diogo/Zith/tests/test-cli-commands.cpp`
- `/home/diogo/Zith/tests/test-docs.cpp` (novo)
- `/home/diogo/Zith/CMakeLists.txt`
- `/home/diogo/Zith/README.md`
- `/home/diogo/Zith/docs/impl-status.md`

Manter a extração do modelo e a renderização independentes do despacho CLI.
Não modificar o frontend para duplicar metadados que `FrontendSnapshot`,
`ModuleArtifact` e `CompilationSnapshot` já expõem. Se uma informação
necessária não estiver disponível, registar um erro de geração em vez de
omitir silenciosamente o símbolo.

## Estimativa

- Markdown, incluindo testes, documentação e escrita segura em ficheiros:
  **5 a 8 dias úteis focados**.
- JSON com schema versionado e testes de paridade semântica:
  **mais 2 a 4 dias úteis focados**, numa fase posterior.
- Total das duas fases: **7 a 12 dias úteis**, aproximadamente **1,5 a 2,5
  semanas** de trabalho focado.
- A maior incerteza é completar assinaturas e metadados de todos os tipos de
  declaração, especialmente macros, context/state e reexports de dependências.
  A associação de comentários tem suporte explícito no frontend atual.

## Execução detalhada

### Passo 1. Adicionar o contrato de opções de `docs`

**Objetivo:** `zithc docs` reconhece as opções acordadas, aplica `interface`
por omissão e deteta combinações inválidas antes de iniciar a compilação.

**Subpassos:**
1. Em `/home/diogo/Zith/src/cli/options.hpp`, adicionar campos dedicados às
   opções do comando `docs`. Não reutilizar `outputFile`, pois esse campo serve
   o output de build.
2. Em `/home/diogo/Zith/src/cli/options.cpp`, dentro de `Cli::parseArgs`,
   reconhecer `--interface`, `--spec`, `--mode=interface`,
   `--mode=spec`, `--out` e `--out=PATH`, `--index`, `--error` e `--force`.
3. Tratar `--mode=interface|spec` antes do parser existente de
   `--mode <debug|dev|release|fast|small>`. Não alterar o parser dos modos de
   compilação dos restantes comandos.
4. Rejeitar `--index` sem `--out`, valores desconhecidos em
   `--mode=...`, caminhos vazios em `--out=`, e seleções contraditórias de
   modo. Repetições do mesmo modo são válidas.
5. Atualizar `tests/test-cli-commands.cpp` com casos para defaults, cada forma
   de modo, combinações contraditórias e validação de `--index`.

**Comandos de verificação:**

```bash
cmake --build /home/diogo/Zith/build --target test-cli-commands -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-cli-commands$' --output-on-failure
```

**Resultado esperado:** ambos os comandos terminam com exit code `0`; os
testes confirmam modo default `interface` e rejeição dos argumentos
contraditórios.

**Falhas e recuperação:**
- Se `--mode=interface` for consumido pelo parser de modo global, mover a
  deteção específica de `docs` para antes desse ramo e repetir os dois
  comandos.
- Se `--out` consumir o caminho do input como valor, alterar o parsing para
  distinguir a flag sem valor de `--out=PATH`; não mudar silenciosamente a
  sintaxe acordada.
- Se `--index` sem `--out` passar, corrigir a validação antes de avançar.

**Critério de sucesso:** `test-cli-commands` passa e a validação do parser não
altera o comportamento de `--mode debug` nos comandos existentes.

### Passo 2. Resolver o projeto, entry point e grafo alcançável

**Objetivo:** o comando escolhe a mesma raiz/entry point de `build` e
`check`, e a documentação contém apenas módulos alcançáveis a partir dessa
raiz.

**Subpassos:**
1. Em `Cli::dispatch()` no ficheiro
   `/home/diogo/Zith/src/cli/options.cpp`, incluir `Command::Docs` no ramo que
   injeta `config.projectRoot` em `opts.inputFiles` quando o projeto foi
   descoberto e o utilizador não passou um input.
2. Em `docs()`, reutilizar a sessão de compilação e o entry configurado em
   `ZithProject.toml`; para input isolado, usar o caminho explícito.
3. Construir o conjunto de módulos documentáveis a partir de
   `CompilationSnapshot::rootModuleKey()` e `CompilationSnapshot::importGraph()`.
   Percorrer arestas de import/export alcançáveis, sem enumerar ficheiros
   órfãos do `src_dir`.
4. Guardar para cada módulo a sua origem e se é módulo do projeto ou
   dependência, para aplicar as regras de `interface` e `spec`.
5. Em `tests/test-docs.cpp`, criar um projeto temporário com entry point,
   dependência transitiva e fonte órfã. Confirmar que só entry e dependência
   alcançáveis aparecem.

**Comandos de verificação:**

```bash
cmake --build /home/diogo/Zith/build --target test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-docs$' --output-on-failure
```

**Resultado esperado:** o teste do projeto fixture passa, inclui o entry e os
módulos importados e exclui a fonte órfã.

**Falhas e recuperação:**
- Se a sessão interpretar o diretório de projeto de forma diferente do entry
  de `ZithProject.toml`, inspecionar a resolução já usada por `buildOneFile()`
  em `/home/diogo/Zith/src/cli/cmd/build.cpp` e passar a mesma entrada à
  `CompilationSession`.
- Se diretórios ou headers C aparecerem como módulos Zith, filtrar
  `ImportEdge::targetKind` e reportar a informação estrangeira na secção de
  imports correspondente, sem tentar documentar uma declaração Zith
  inexistente.
- Se o módulo órfão surgir, não usar `collectFiles()` como lista de módulos.
  Usar apenas o grafo do snapshot.

**Critério de sucesso:** o fixture prova por asserções que o conjunto gerado é
exatamente o fecho transitivo Zith do entry point.

### Passo 3. Extrair o modelo semântico de documentação

**Objetivo:** cada módulo alcançável tem um modelo ordenado de declarações com
assinaturas, tipos, visibilidade, origem e comentários.

**Subpassos:**
1. Criar `/home/diogo/Zith/src/cli/docs/doc-model.hpp` e `.cpp`.
2. Implementar um extrator que recebe o snapshot e o conjunto de módulos
   selecionado. Consultar `ModuleArtifact::frontend`, `publicSymbols`,
   `moduleSymbols`, `CompilationSnapshot::mergedSymbols()` e as resoluções
   disponíveis, sem alterar os objetos do frontend.
3. Mapear todas as categorias acordadas: funções, externs, macros, imports,
   exports, aliases, structs, enums, unions, traits, interfaces, context e
   state. Preservar variantes, campos, parâmetros, tipos e genéricos existentes
   no AST.
4. Associar `TriviaKind::DocLine` e `TriviaKind::DocBlock` à declaração usando
   os spans e o leading trivia do token inicial. Remover delimitadores e
   normalizar a indentação sem alterar o Markdown interno.
5. Aplicar a seleção: `interface` inclui públicos e reexports; `spec` inclui
   todos os símbolos dos módulos do projeto e apenas símbolos de dependências
   usados ou reexportados.
6. Ordenar módulos e declarações por chave de módulo, nome, assinatura e span,
   de modo que entradas equivalentes produzam sempre a mesma saída.
7. Adicionar testes unitários do modelo para documentação, overloads,
   genéricos, visibilidade, reexports e categorias de declaração.

**Comandos de verificação:**

```bash
cmake -S /home/diogo/Zith -B /home/diogo/Zith/build
cmake --build /home/diogo/Zith/build --target test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-docs$' --output-on-failure
```

**Resultado esperado:** a configuração atualiza a descoberta recursiva de
fontes, o alvo compila e os testes do modelo passam.

**Falhas e recuperação:**
- Se um tipo de declaração não tiver campos suficientes no AST, adicionar ao
  modelo uma representação explícita de indisponibilidade e produzir
  diagnóstico de geração. Não inventar assinatura nem descartar o símbolo.
- Se comentários não estiverem no leading trivia do token esperado, usar os
  spans de `FrontendSnapshot::trivia()` para encontrar a associação imediata
  anterior à declaração e acrescentar teste específico.
- Se a saída variar, identificar o container não ordenado e ordenar antes de
  renderizar.

**Critério de sucesso:** os testes verificam as categorias e metadados
acordados e comparam igualdade byte a byte entre duas extrações equivalentes.

### Passo 4. Implementar a política de erros

**Objetivo:** erros bloqueiam saída parcial por omissão; `--error` inclui o
conteúdo válido e uma secção de erros com localização por ficheiro, módulo e
secção.

**Subpassos:**
1. Recolher diagnostics da `CompilationSession` e erros do extrator sem
   imprimir diretamente durante a geração.
2. Converter cada erro para um registo com ficheiro, módulo, secção e mensagem.
3. Sem `--error`, devolver exit code `1` e não escrever Markdown no terminal
   nem modificar ficheiros de saída.
4. Com `--error`, renderizar as declarações válidas, acrescentar `## Errors`
   com grupos estáveis por ficheiro, módulo e secção e devolver exit code `1`.
5. Testar erros de parsing, erro de import e erro de geração em fixture
   temporário, verificando tanto a ausência de saída como o conteúdo parcial
   explicitamente solicitado.

**Comandos de verificação:**

```bash
cmake --build /home/diogo/Zith/build --target test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-docs$' --output-on-failure
```

**Resultado esperado:** casos sem erro terminam em `0`; casos com erro
terminam em `1`; só `--error` contém secções válidas e `## Errors`.

**Falhas e recuperação:**
- Se o buffer de sessão for impresso antes de conhecer o estado final,
  guardar diagnostics e texto até à decisão de política.
- Se `--error` devolver `0`, manter exit code `1`, mesmo quando os erros foram
  incluídos como texto.
- Se o modo default deixar ficheiros parcialmente atualizados, calcular e
  validar primeiro todo o modelo e toda a saída antes de iniciar a escrita.

**Critério de sucesso:** testes comprovam que um erro nunca produz saída
parcial por omissão e que `--error` relata a secção que falhou.

### Passo 5. Renderizar o Markdown agregado

**Objetivo:** uma compilação válida produz um `API.md` legível, completo e
determinístico.

**Subpassos:**
1. Criar `/home/diogo/Zith/src/cli/docs/markdown-renderer.hpp` e `.cpp`.
2. Implementar uma renderização pura do modelo, sem escrever em `stdout`,
   `stderr` ou no filesystem.
3. No modo agregado, começar com o entry point, apresentar o grafo alcançável
   e depois organizar símbolos por módulo e secção.
4. Renderizar assinaturas completas, tipos, campos, variantes, parâmetros
   genéricos, visibilidade, origem e comentários normalizados.
5. Distinguir explicitamente símbolos locais de símbolos reexportados e
   apresentar o módulo de origem.
6. Em `tests/test-docs.cpp`, comparar a saída esperada completa para um
   fixture pequeno e comparar duas renderizações para determinismo.

**Comandos de verificação:**

```bash
cmake -S /home/diogo/Zith -B /home/diogo/Zith/build
cmake --build /home/diogo/Zith/build --target test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-docs$' --output-on-failure
```

**Resultado esperado:** `test-docs` passa e confirma títulos, assinaturas,
comentários, origem e ordem determinística.

**Falhas e recuperação:**
- Se Markdown de comentários criar cabeçalhos estruturais inválidos, preservar
  literalmente o conteúdo normalizado e não escapar Markdown válido.
- Se overloads com o mesmo nome colidirem, incluir a assinatura na âncora e
  na ordenação.
- Se reexports parecerem declarações locais, marcar a origem no modelo antes
  da renderização.

**Critério de sucesso:** o teste de golden Markdown passa sem dependência de
ordem de filesystem ou stdout.

### Passo 6. Implementar `--out`, `--index` e `--force`

**Objetivo:** o utilizador pode escrever `API.md` ou um índice multipágina,
sem sobrescrever destinos existentes sem consentimento.

**Subpassos:**
1. Criar `/home/diogo/Zith/src/cli/docs/output-writer.hpp` e `.cpp`.
2. Resolver `--out` para `<project-root>/docs/` ou, para ficheiro isolado,
   `<source-parent>/docs/`. Resolver `--out=PATH` para o diretório indicado.
3. Escrever o Markdown agregado em `API.md`.
4. Para `--index`, escrever `README.md` e `modules/<module-key-seguro>.md`.
   Usar links relativos entre índice, módulos e símbolos reexportados.
5. Calcular toda a lista de caminhos e detetar colisões antes de criar ou
   substituir destinos.
6. Se qualquer destino já existir sem `--force`, falhar antes de modificar
   qualquer destino. Com `--force`, substituir somente os ficheiros desta
   geração. Não remover ficheiros adicionais no diretório.
7. Testar `--out`, defaults de projeto e input isolado, índice, colisões,
   overwrite sem `--force`, overwrite com `--force` e preservação de um
   ficheiro alheio.

**Comandos de verificação:**

```bash
cmake -S /home/diogo/Zith -B /home/diogo/Zith/build
cmake --build /home/diogo/Zith/build --target test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^test-docs$' --output-on-failure
```

**Resultado esperado:** os testes confirmam os caminhos exatos, a exigência de
`--force` e que o ficheiro alheio permanece byte a byte inalterado.

**Falhas e recuperação:**
- Se `--index` funcionar sem `--out`, corrigir a validação do Passo 1.
- Se dois módulos gerarem o mesmo nome seguro, cancelar a geração antes de
  escrever e apresentar os dois módulos em conflito.
- Se um overwrite falhar depois de substituir apenas parte dos ficheiros,
  rever o writer para preparar todos os conteúdos, validar todos os destinos
  e só depois iniciar a atualização.

**Critério de sucesso:** todos os testes de filesystem passam e não existe
remoção de ficheiros fora do conjunto explicitamente gerado.

### Passo 7. Ligar a geração ao comando e atualizar a ajuda

**Objetivo:** o dispatch `zithc docs` usa o gerador e documenta as opções
suportadas sem alterar os outros comandos.

**Subpassos:**
1. Substituir a impressão direta atual em `docs()` de
   `/home/diogo/Zith/src/cli/cmd/tool.cpp` por chamadas ao modelo,
   diagnostics, renderer e writer.
2. Manter mensagens de erro e status do comando em `stderr`; sem `--out`,
   escrever apenas o documento em `stdout`.
3. Atualizar a ajuda em `/home/diogo/Zith/src/cli/cmd/info.cpp` com modo,
   destino, índice, erros e force.
4. Atualizar as completações de Bash, Zsh e Fish em
   `/home/diogo/Zith/src/cli/cmd/completion.cpp`.
5. Atualizar a referência curta de comandos em `/home/diogo/Zith/README.md` e
   o status do comando em `/home/diogo/Zith/docs/impl-status.md`.
6. Adicionar `test-docs` a `/home/diogo/Zith/CMakeLists.txt`, seguindo
   `add_zith_test`.
7. Executar um smoke test com projeto fixture e com um ficheiro isolado.

**Comandos de verificação:**

```bash
cmake -S /home/diogo/Zith -B /home/diogo/Zith/build
cmake --build /home/diogo/Zith/build --target zithc test-cli-commands test-docs -j 4
ctest --test-dir /home/diogo/Zith/build -R '^(test-cli-commands|test-docs)$' --output-on-failure
/home/diogo/Zith/build/zithc --help
```

**Resultado esperado:** os alvos compilam, ambos os testes passam e `--help`
lista as opções de `docs`.

**Falhas e recuperação:**
- Se `zithc` não existir porque o build foi configurado sem CLI, registar a
  opção CMake ativa, configurar com `-DZITH_BUILD_CLI=ON` e repetir.
- Se as completações divergirem da ajuda, alinhar as três listas com o parser.
- Se os novos `.cpp` não forem compilados, confirmar que a configuração CMake
  foi repetida depois da criação dos ficheiros.

**Critério de sucesso:** o comando apresenta `API.md` no terminal por omissão,
escreve ficheiros apenas com `--out` e passa os dois testes focados.

### Passo 8. Verificação final e atualização do estado

**Objetivo:** o contrato Markdown está coberto, documentado e verificável no
build local.

**Subpassos:**
1. Executar os testes focados e a verificação de formato.
2. Se `fmt-check` identificar ficheiros novos, formatar somente os ficheiros
   desta tarefa com `clang-format`, sem formatar alterações alheias.
3. Executar novamente os testes após qualquer formatação.
4. Rever `git diff --check` e `git status --short`.
5. Confirmar que a saída contém apenas ficheiros previstos e que alterações
   pré-existentes continuam intactas.
6. Só depois de cada tarefa da Sequenta passar os respetivos testes, usar
   `checkpoint` e `verify` para a marcar `done`. Não marcar tarefas `done`
   durante a gravação inicial deste plano.

**Comandos finais:**

```bash
cmake --build /home/diogo/Zith/build --target test-cli-commands test-docs fmt-check -j 4
ctest --test-dir /home/diogo/Zith/build -R '^(test-cli-commands|test-docs)$' --output-on-failure
git -C /home/diogo/Zith diff --check
git -C /home/diogo/Zith status --short
```

**Resultado esperado:** todos os comandos de teste e formato terminam com
exit code `0`, `git diff --check` não reporta whitespace inválido e o estado
do worktree não perde alterações pré-existentes.

**Falhas e recuperação:**
- Se um teste falhar, corrigir a implementação correspondente e repetir esse
  alvo antes de atualizar Sequenta.
- Se aparecer uma alteração fora da lista prevista, inspecionar o diff e
  preservá-la. Não a descartar automaticamente.
- Se `fmt-check` falhar em ficheiros preexistentes não relacionados, registar
  esses caminhos como falha preexistente e não os formatar.

**Critério de sucesso:** os testes, formato e revisão de diff terminam sem
falhas novas, e as tarefas Sequenta só ficam `done` depois de verificadas.

## Fase posterior: JSON

Implementar apenas depois de a entrega Markdown estar aceite. Adicionar
`--format=md|json` e `--json`, definir um schema com versão explícita, gerar o
JSON a partir do mesmo modelo semântico e testar que Markdown e JSON contêm a
mesma informação. Estimativa separada: 2 a 4 dias úteis focados. Não alterar o
contrato Markdown para antecipar detalhes de serialização JSON.

## Aceitação final

- `zithc docs` sem flags usa `interface` e imprime um único documento Markdown
  no terminal.
- `--spec` inclui símbolos privados dos módulos do projeto alcançáveis.
- O entry point e o grafo de imports/exports determinam os módulos incluídos;
  fontes órfãs ficam de fora.
- `--out` e `--out=PATH` escrevem `API.md`; `--index` escreve `README.md` e
  páginas por módulo.
- Destinos existentes exigem `--force`, e ficheiros não gerados nunca são
  apagados.
- Erros não produzem saída parcial sem `--error`; com `--error`, erros são
  agrupados por ficheiro, módulo e secção e o exit code continua a ser `1`.
- Comentários, assinaturas, genéricos e categorias de declaração acordadas
  aparecem de forma determinística.
- Os testes `test-cli-commands` e `test-docs` passam, e `fmt-check` não deteta
  problemas introduzidos pela tarefa.
