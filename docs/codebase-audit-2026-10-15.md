# Auditoria Técnica da Base de Código

Auditoria estrutural do workspace Zith em 2026-10-15. As notas são avaliações
qualitativas, não métricas automatizadas de qualidade. O relatório separa
defeitos observados, riscos inferidos e pontos que não foram verificados.

## Escopo e Método

A inspeção cobriu a organização de `src/`, documentação de arquitetura e
implementação, configuração CMake/CI, testes e amostras de código. O workspace
estava sujo durante a análise: havia alterações locais em documentos, código e
testes. As conclusões sobre arquivos referem-se ao estado do workspace, não a
uma árvore limpa de um commit.

O levantamento encontrou 61.302 linhas em 213 arquivos `.cpp`/`.hpp` de
`src/`, 43 arquivos `.cpp` em `tests/` e 24.439 linhas nesses testes. `ctest
--test-dir build -N` listou 42 testes registrados, incluindo o check de
consistência documental. As 3.065 ocorrências de `CHECK` encontradas são locais
de chamada da macro, não uma medição de cobertura nem necessariamente o número
de verificações executadas.

Foram inspecionados arquivos representativos e pesquisados sinais estruturais.
`cppcheck` em `src/memory/` e `src/diagnostics/` excedeu o limite de 300
segundos. Em execuções isoladas, `arena.cpp` e `flat-map.hpp` não tiveram
achados; outras unidades produziram avisos `uninitMemberVarNoCtor` em tipos de
dados agregados. Esses avisos não foram tratados como defeitos confirmados:
há usos agregados, e `docs/implementation-debt.md` já classifica ocorrências
desse padrão como falsos positivos verificados.

Não foram executados build, testes, sanitizers, fuzzers, análise de cobertura ou
benchmarks nesta auditoria. Portanto, as notas não representam validação
dinâmica nem uma conclusão de desempenho.

## Resumo Executivo

- **Score geral:** 7,2/10, média de 8, 8, 7, 6 e 7.
- **Status:** Bom.
- **Principal ponto forte:** pipeline modular, ownership explícito com arenas,
  documentação de implementação detalhada e testes de integração abrangentes.
- **Maior risco técnico identificado:** o parser recursivo não apresenta um
  limite de profundidade visível nos pontos inspecionados. Entrada
  profundamente aninhada pode esgotar a pilha do processo. É um risco inferido
  da implementação, não uma falha reproduzida nesta auditoria.

## Avaliação por Pilar

### Legibilidade e Manutenibilidade

- **Nota:** 8/10.
- **Pontos positivos:** `.clang-format` estabelece estilo consistente; o
  projeto tem documentação de implementação, dívida técnica e decisões de
  arquitetura; a busca encontrou um `TODO` de código em `src/` e nenhum
  `FIXME` ou `HACK`.
- **Débito confirmado:** `CONTRIBUTING.md` descreve `src/ast`, `src/lexer`,
  `src/parser` e `src/output`, diretórios que não existem na árvore atual. O
  frontend ativo está em `src/frontend/`, e o formato binário está em
  `src/zirl/`.
- **Débito menor:** dois comentários informais e com erros de escrita ainda
  aparecem em `src/diagnostics/error-codes.hpp`.
- **Contexto:** 11 arquivos de implementação ultrapassam 1.000 linhas. Isso
  aumenta o custo de navegação, mas, por si só, não demonstra baixa coesão ou
  necessidade de novo split. Os splits concluídos e as responsabilidades estão
  listados em `docs/plans/monolith-splits.md`.
- **Evidência:** `src/ide/lsp-ide-api.cpp` tem 1.736 linhas;
  `src/sema/hir-lower-expr-value.cpp` tem 1.703. Os dois diretórios citados
  como ausentes não aparecem em `src/`.

### Arquitetura e Design

- **Nota:** 8/10.
- **Pontos positivos:** o fluxo separa frontend, resolução, sema, HIR, codegen,
  cache e interfaces; `CompilationSession` coordena a compilação; `Arena` e
  contêineres move-only estabelecem padrões explícitos de vida útil; CMake
  habilita ou remove subsistemas por opção.
- **Débito observado:** alguns headers concentram estado e APIs compartilhadas,
  em particular `src/sema/sema-modern.hpp` e
  `src/session/frontend-context.hpp`. `compilation-session.hpp` inclui vários
  subsistemas, elevando o acoplamento de compilação.
- **Contexto:** CMake usa `file(GLOB_RECURSE ...)`; após adicionar arquivos
  `.cpp`, é necessário reconfigurar para atualizar a lista de fontes. Esse
  comportamento está documentado no projeto.
- **Evidência:** `src/sema/sema-modern.hpp` tem 779 linhas e
  `src/session/frontend-context.hpp` tem 601. O cabeçalho de
  `CompilationSession` inclui diretamente headers de cache, sema, HIR,
  símbolos e tipos.

### Performance e Eficiência

- **Nota:** 7/10, com baixa confiança por falta de medições.
- **Pontos positivos:** `memory::Arena` verifica alinhamento e limites de
  alocação; `FlatMap` usa hashing e sondagem linear; o código usa arenas e
  contêineres do projeto em partes centrais do compilador.
- **Débito de evidência:** não foram encontrados resultados de benchmark ou
  profiling que estabeleçam gargalos ou regressões. A nota é estrutural e não
  afirma que o compilador seja rápido ou lento.
- **Candidato a medir:** `LinearMemory::read` cria e retorna um `vector` por
  leitura em `src/vm/vm-memory.cpp`. Sem perfil de uso, isso não está
  classificado como gargalo nem como refatoração necessária.
- **Correção ao relatório anterior:** a contagem de 276 parâmetros `std::string`
  por valor não foi validada. O comando usado contava declarações que
  correspondiam ao padrão textual, não parâmetros de função; essa afirmação foi
  retirada.

### Segurança e Robustez

- **Nota:** 6/10.
- **Pontos positivos:** leituras e escritas de `LinearMemory` verificam limites
  antes de copiar; parsing de inteiros detecta overflow; expansão de macros
  limita a profundidade a 64; o caminho de link usa `execvp` sem montar uma
  string de shell.
- **Risco principal:** `AstLowerer::parseExpression`, `parseType` e o parsing
  de blocos/condicionais possuem caminhos recursivos. A inspeção não encontrou
  limite geral de profundidade no parser, embora haja um limite separado para
  expansão de macros. Isso pode permitir crash por esgotamento de pilha com
  entrada aninhada. A reprodução e o comportamento de recuperação precisam de
  testes antes de classificar o risco como falha confirmada.
- **Débito de verificação:** a busca por configuração não encontrou job de
  sanitizer, fuzzing ou cobertura em `.github/workflows/` e `CMakeLists.txt`.
  A CI executa builds e testes, mas isso não substitui essas análises.
- **Avisos estáticos:** os avisos de campos sem inicializador não foram
  classificados como bugs sem evidência de leitura antes de escrita.
- **Correção ao relatório anterior:** o alerta sobre `strtod` não foi
  sustentado por análise completa do fluxo de validação e foi removido dos
  achados. Não se recomenda alterar a conversão até existir um caso de falha
  reproduzível.

### Testabilidade

- **Nota:** 7/10.
- **Pontos positivos:** 42 testes estão registrados no CTest; há testes
  direcionados para frontend, sema, codegen, cache, VM, WASM e CLI. Os testes
  cobrem diagnósticos e casos negativos, além dos caminhos aceitos.
- **Débito observado:** `tests/test-common.hpp` fornece um harness próprio com
  `CHECK`, `CHECK_EQ` e `TEST_MAIN`. É simples e determinístico, mas não oferece
  fixtures, filtros ou parametrização de uma biblioteca de testes.
- **Limite da evidência:** não há medição de cobertura configurada. As
  ocorrências de `CHECK` não demonstram quais ramos foram executados.
- **Evidência:** `tests/test-frontend.cpp` testa diretamente
  `frontend::parse`; `tests/test-generic-constraints.cpp` verifica códigos e
  contagem de diagnósticos; `tests/test-common.hpp` implementa o harness
  compartilhado.

## Plano de Ação

| Prioridade | Arquivo/Módulo | Problema ou risco | Ação recomendada |
| :--- | :--- | :--- | :--- |
| Alta | `src/frontend/ast-lowerer.hpp`, `src/frontend/frontend-expr.cpp`, `src/frontend/frontend-types.cpp`, `src/frontend/frontend-stmt.cpp` | Recursão de parsing sem limite geral confirmado | Definir limite compartilhado de profundidade, emitir diagnóstico de parsing e cobrir entradas aninhadas no teste de frontend |
| Média | `.github/workflows/ci.yml` | CI não executa sanitizers | Adicionar job Debug com ASan/UBSan e executar a suíte CTest sem `allow-failure` |
| Média | Frontend e CI | Não há fuzz target identificado | Após o limite de profundidade, avaliar um alvo libFuzzer para `frontend::parse`, com tempo limitado na CI e seeds de testes existentes |
| Baixa | `src/sema/sema-modern.hpp`, `src/session/frontend-context.hpp`, `src/ide/lsp-ide-api.cpp` | Arquivos grandes e APIs compartilhadas | Revisar coesão e interfaces quando houver mudança funcional, sem iniciar splits apenas por contagem de linhas |
| Feito nesta documentação | `CONTRIBUTING.md`, `AGENTS.md`, memórias de auditoria | Organização, orientação de memória e estado da auditoria estavam desatualizados | Alinhar documentação com a árvore e o estado atual do workspace |

## Limitações

Esta auditoria não substitui code review por subsistema. O parser foi inspecionado
para identificar recursão, mas não foi submetido a uma entrada de stress. Não
foram executados build/CTest, ASan/UBSan, fuzzing, cobertura, clang-tidy ou
benchmarks. O cppcheck completo excedeu o timeout em duas pastas e os resultados
isolados não permitem declarar a base inteira limpa.
