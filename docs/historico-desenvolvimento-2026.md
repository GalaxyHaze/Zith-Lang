# Histórico de desenvolvimento do Zith, julho a setembro de 2026

Este relatório consolida as mudanças commitadas entre 13 de julho e 30 de
setembro de 2026, organizadas em cinco fases temáticas.

## Âmbito e método

- Início: `b2551156b3bf6ed197a3c6de60c23f81d87eaf6d`, em 13/07/2026 às
  13:28:27 +01:00, `feat: update CLI, runtime, and compiler support work`.
- Fim: `e5b8955b6d5add193a2f113443aadffe9ee76415`, em 30/09/2026 às
  13:42:00 +01:00, `fix: avoid cross-kind generic type reuse`.
- O intervalo contém 267 commits alcançáveis a partir do HEAD local. A data
  deste relatório é 01/10/2026, mas não há commit posterior ao de 30/09 no
  HEAD analisado.
- As fases são uma divisão temática deste relatório, não marcos oficiais do
  projeto.
- As colunas de linhas somam inserções e remoções de cada commit não-merge
  dentro da fase. Os merges contam no número de commits, mas os respetivos
  diffs não são somados. Reescritas sucessivas da mesma área contam mais de
  uma vez. Ficheiros binários sem contagem textual não entram nestes totais.
- Como comparação do estado final, o diff entre o pai de `b2551156` e
  `e5b8955b` apresenta 111.455 inserções e 12.669 remoções em 659 ficheiros.
  Esse delta compara apenas os dois estados, ao contrário das somas por
  commit da tabela.

| Fase | Período aproximado | Commit inicial | Commit final | Commits | Linhas adicionadas | Linhas removidas |
|---|---|---|---|---:|---:|---:|
| 1. Base do compilador e portabilidade | 13/07–26/07 | `b2551156` | `3bca7eb4` | 25 | 19.635 | 7.396 |
| 2. Fundação da frontend moderna | 27/07–06/08 | `cb517535` | `a5f37169` | 13 | 30.254 | 4.419 |
| 3. Expansão da linguagem e integrações | 07/08–07/09 | `6dbff2db` | `71f1997d` | 134 | 82.652 | 50.342 |
| 4. Interpreter, execução e consolidação semântica | 08/09–25/09 | `bd3878b5` | `06682611` | 56 | 22.758 | 5.572 |
| 5. VM WASM e distribuição | 26/09–30/09 | `a0064570` | `e5b8955b` | 39 | 13.039 | 1.559 |
| **Total** | **13/07–30/09** | `b2551156` | `e5b8955b` | **267** | **168.338** | **69.288** |

## Fase 1. Base do compilador e portabilidade

**13 a 26 de julho.** A fase começa com o trabalho no CLI, runtime e suporte
do compilador, e termina em `3bca7eb4`.

- O pipeline de imports e a dispatch do typed AST foram decompostos em partes
  menores. O parser teve duplicação reduzida e passou a registar resultados
  tipados.
- A verificação de tipos foi separada do lowering para HIR, criando uma
  fronteira mais clara entre sema e geração de HIR.
- LLVM passou a ser opcional em builds locais. Foram corrigidos problemas de
  compatibilidade de Clang, WASM e musl, além de ajustes em CMake, CI e
  distribuição Windows/Scoop.
- Foi introduzida uma barreira semântica para sintaxe experimental, com
  atualizações ao README, ao estado de implementação e à documentação da
  linguagem.
- A fase inclui vários commits com mensagens genéricas, como `updates` e
  `A lot haha`. O histórico não permite atribuir com segurança uma lista
  detalhada de alterações a cada um desses commits.

## Fase 2. Fundação da frontend moderna

**27 de julho a 6 de agosto.** A fase parte de `cb517535`, que introduz um AST
com IDs e uma tabela de tipos moderna, e termina em `a5f37169`.

- Foi criada a base de frontend com IDs para declarações, expressões,
  statements e scopes, incluindo suporte inicial a `if` e `while`.
- O contexto de frontend foi migrado para `FrontendSnapshot`, e o código
  legado foi separado da árvore moderna.
- A pipeline moderna foi ligada a HIR e sema, com refatoração do cache e
  criação de testes para C interop.
- Foram adicionados operadores com múltiplos caracteres, casts explícitos
  `as`, ponteiros não anuláveis e testes de `null`.
- A linguagem ganhou literais de arrays, `@sizeOf`, expressões `when/match`
  com ranges e casos default, loops `for` de três cláusulas e parâmetros
  genéricos em funções, structs e aliases.
- Macros e factos NRA foram integrados na frontend moderna, junto com mais
  ferramentas e cobertura de testes.

## Fase 3. Expansão da linguagem e integrações

**7 de agosto a 7 de setembro.** É a fase mais extensa em commits e alterações
textuais. Começa com correções de builds de release e termina em `71f1997d`.

- Foram implementados marcadores de fluxo, incluindo armazenamento TLS e uma
  forma de fluxo sem pilha.
- A sema moderna foi exposta ao LSP. Foi criada a fachada `zith::ide` v1,
  com contrato, schema, testes e overlays para documentos em memória.
- A semântica de bindings e constantes foi ampliada com propagação de
  imutabilidade, globais `const`, discriminantes de enums e casts de enums no
  codegen.
- Foram adicionados iteradores `for-in`, o protocolo `End`, `Counter`, ranges,
  o operador `in`, `Contains` e loops sobre ranges.
- Traits e interfaces ganharam conformance, bounds genéricos, acesso a campos
  e métodos, dispatch dinâmico, packs e anotações `lend`/`view`. Também foram
  implementados cleanup com `defer` e slices variádicos `[...]T`.
- O C interop passou a importar constantes de macros object-like. Foram
  ampliados os tipos opacos, a identidade estrutural de packs, optionals
  aninhados e a hidratação de tipos através do cache.
- Foram adicionadas importações específicas por plataforma e uma fatia
  validada de ABI C para structs simples passadas por valor.
- A biblioteca padrão ganhou `Formatable`, `print`, `println` e `input`, com
  lowering para primitivas e slices dinâmicos e suporte a chamadas de
  formatação.
- O período também incluiu refatorações do frontend, sema, sessão e codegen,
  atualização de exemplos e auditorias de releases, instaladores, LLVM e
  descoberta da biblioteca padrão em várias plataformas.

## Fase 4. Interpreter, execução e consolidação semântica

**8 a 25 de setembro.** A fase começa em `bd3878b5` e termina em `06682611`.

- Foram definidos contratos para Execution IR e execução interpretada. Foi
  implementado um interpreter de HIR, ligado a `zithc run --interpreted`, e
  uma VM de Execution IR com lowering a partir de HIR.
- A linguagem ganhou os operadores `pipe` e `do`. O DAG passou a diagnosticar
  ciclos explicitamente.
- Foram adicionados diagnósticos de overflow para casts numéricos, regras
  explícitas para descartar resultados de chamadas e verificações de
  dereference de ponteiros anuláveis, incluindo narrowing após `is null`.
- Passaram a ser permitidos bindings tipados sem inicializador até ao
  primeiro uso. Foram introduzidos atributos MVP para `discardable` e
  `volatile`, incluindo a aplicação de `discardable` a `print` e `println`.
- Houve melhorias em genéricos e hashmap, estabilidade de tipos opacos entre
  módulos, importações, ABI e documentação das decisões de Execution IR.
- O parser de expressões e o lowering de expressões HIR foram divididos em
  unidades mais focadas.

## Fase 5. VM WASM e distribuição

**26 a 30 de setembro.** A fase começa em `a0064570` e termina no HEAD
`e5b8955b`.

- Foi adicionado um script local de build WASM e a ABI de execução da VM v2.
  O pacote WASM da biblioteca padrão foi integrado no playground.
- A VM recebeu `CallRange`, uma intrinsic de impressão, operadores bitwise e
  shifts, suporte a chamadas `extern` variádicas e `realloc`.
- Foram ampliados os emitters do compilador e a superfície do runtime C.
- O pipeline de distribuição foi endurecido com validações de assets e
  instaladores, separação de manutenção de releases e smoke tests, além de
  correções para Windows, musl, LLVM e compilação cross-target.
- O commit final corrigiu a reutilização indevida de tipos genéricos entre
  categorias diferentes.

## Registos de uso de tokens

Não foi encontrado um registo fiável de tokens de modelo usados por fase,
commit ou pelo projeto neste intervalo. As contagens do Git medem linhas de
texto, não tokens. Os metadados locais consultados incluem bytes estimados,
mas não fornecem contagens de tokens associadas aos commits. Menções a tokens
em mensagens de CI ou de configuração também não são métricas de utilização.

## Limites desta consolidação

O relatório agrupa as funcionalidades e correções descritas pelas mensagens
dos commits. Não reproduz os 267 títulos individualmente. Mensagens genéricas,
commits de merge e alterações cujo título não explica o conteúdo limitam a
granularidade possível sem fazer uma revisão arquivo a arquivo de todos os
diffs.
