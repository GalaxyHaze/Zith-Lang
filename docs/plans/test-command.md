# Plano: o comando `zithc test` e o teste nativo Zith--

## Objetivo

Transformar `zithc test` de um type-check em lote de ficheiros `.zith` num
runner de testes nativo. O comando descobre funções `#[test]` sob `test_dir`,
compila cada ficheiro com um driver sintetizado pelo compilador, executa-o em
qualquer backend e reporta o resultado por teste, incluindo a localização do
assert que falhou. A iteração adiciona o atributo `#[test]` e os dois
intrínsecos `@assert` e `@log` ao Zith--.

## Pré-condições

- `cwd` é `/home/diogo/Zith`.
- O build de desenvolvimento está configurado em `/home/diogo/Zith/build`.
- As fontes da biblioteca são descobertas por `file(GLOB_RECURSE)` no
  `/home/diogo/Zith/CMakeLists.txt`. Depois de adicionar `.cpp` novos, voltar a
  configurar o build para atualizar o glob.
- Preservar todas as alterações existentes no worktree. Não editar nem
  reverter alterações alheias a este plano.
- Não executar `cmake --build build --target fmt`, porque pode reformatar
  ficheiros sem relação com esta tarefa. Usar `fmt-check`.
- Não executar `git reset --hard`, `git checkout --` ou comandos que apaguem
  ficheiros do utilizador.
- A implementação atual de `test()` está em
  `/home/diogo/Zith/src/cli/cmd/tool.cpp`. Descobre `*.zith` sob `test_dir` e
  corre-os até `session::Stage::TypeChecked` com `runOnFiles`/`countPassed` de
  `/home/diogo/Zith/src/cli/cmd/build.cpp`.
- A skill de execução é Sequenta apenas se o utilizador aprovar explicitamente
  antes do trabalho começar. Sem aprovação, executar de forma linear normal.

## Estado atual

- `zithc test` faz apenas type-check em lote. Não executa nada e não existe
  qualquer mecanismo de asserção. A linha em `docs/impl-status.md` diz
  `Working` com a nota "Discovers and runs test files under the given path",
  que descreve o comportamento atual e não o desenho pretendido.
- O `zithc create` já gera `tests/` e escreve `test_dir = "tests"` em
  `[paths]` do `ZithProject.toml` (`src/cli/cmd/project.cpp`), mas
  `docs/19-project-config.md` não documenta a chave.
- Existe maquinaria de atributos `#[...]`: `AttributeKind` e `AttributeTarget`
  em `src/frontend/frontend.hpp`, resolvidos por `classifyAttribute` em
  `src/frontend/attribute-classify.hpp`. Atributos conhecidos atuais são
  `discardable` (funções) e `volatile` (variáveis).
- Existe maquinaria de intrínsecos `@`: parseados em
  `src/frontend/frontend-expr-primary.cpp`, tipados em
  `src/sema/sema-cast-coerce.cpp` e lowered em
  `src/sema/hir-lower-expr-value.cpp`.
- `HirRuntimePanic` existe no HIR (`src/hir/hir-expr.hpp`) e o codegen nativo
  baixa-o para `trap` (`src/codegen/codegen-emit-expr.cpp`). A VM v2 só suporta
  o código de panic `10003` e o interpretador HIR não trata panics.
- A secção `Debug` do ZIRL (`SectionId::Debug = 8`) está reservada e vazia.
  `encodeDebug`/`decodeDebug` em `src/zirl/zirl-debug-section.cpp` ignoram o
  payload.
- O compilador já tem os dois modos de execução do filho:
  `linkAndExec()` captura stdout e stderr num pipe e `linkAndExecDirect()` herda
  os descritores (`src/session/native-link.cpp`). A função `captureProgram`
  junta stdout e stderr no mesmo pipe.

## Decisões de produto

- A unidade de teste é uma função anotada `#[test]`, não um ficheiro nem um
  módulo. Vários testes cabem no mesmo ficheiro.
- Passar significa compilar, executar e satisfazer as asserções em runtime. O
  comando deixa de ser um type-check.
- A descoberta é por ficheiro: cada `*.zith` sob `test_dir` compila e corre
  sozinho, com o seu próprio driver e o seu próprio processo. `#[test]` só é
  descoberto no próprio ficheiro, nunca em módulos importados. Um ficheiro sem
  `#[test]` é saltado com uma nota, não é erro.
- O driver é sintetizado como HIR pelo compilador, depois do sema, para servir
  codegen nativo, VM v2 e interpretador pelo mesmo caminho.
- O resultado de cada teste vive numa tabela sintetizada pelo compilador com
  uma entrada por função `#[test]`. O valor da entrada é `0` quando o teste
  passa e `N` quando falha no assert de id estático `N` desse teste. A tabela
  é infraestrutura do runner e não um global que o código de utilizador possa
  escrever.
- O driver continua depois de um teste falhar e preenche todos os slots. Um
  trap duro (segfault, divisão por zero) mata o processo e o runner reporta
  crash para esse ficheiro.
- A única primitiva de falha é `@assert(cond)`, sem mensagem. Mensagens ricas
  pertencem à stdlib.
- `@assert` fora de uma função `#[test]` comporta-se como panic. Dentro de um
  teste, `@assert` regista o id estático da falha no slot e termina o teste.
- Um teste que falha num assert retorna de imediato. O resto do corpo não
  corre.
- As funções `#[test]` são `void` e sem parâmetros. `#[test]` sobre uma função
  com parâmetros ou retorno não-void reporta `W2029 AttributeNotApplicable`.
- Um módulo de teste não pode declarar `main` e `#[test]` ao mesmo tempo. A
  colisão é erro.
- O `@log(msg)` acumula texto num buffer sintetizado por teste. Por omissão o
  driver imprime o log apenas dos testes que falham, e `--verbose` mostra o de
  todos. O próprio driver imprime o log, o runner apenas faz relay.
- Os backends suportados são todos: codegen nativo, VM v2 e interpretador.

## Modelo

### Zith test function

Uma função livre anotada `#[test]`, sem parâmetros e com retorno `void`. É a
unidade que o runner descobre e reporta. Não exige um `main` no módulo.

```zith
#[test]
fn adds_two_numbers(): void {
    @assert(1 + 1 == 2);
}
```

### Test driver

Um `main` sintetizado pelo compilador para um ficheiro de teste. Chama cada
função `#[test]` descoberta por ordem de declaração, limpa o buffer de log
antes de cada teste e, no fim, imprime o bloco de resultados. O driver é
construído como HIR, por isso o mesmo desenho serve os três backends.

### Test result table

Uma tabela com uma entrada por função `#[test]`. O valor `0` significa que o
teste passou. O valor `N` significa que o teste falhou no assert de id estático
`N`. A tabela é sintetizada pelo compilador, equivalente a um global mutável
privado, que a linguagem não permite escrever ao utilizador.

### Assertion intrinsic

`@assert(cond)` é a única primitiva de falha. Dentro de um teste, baixa para
`if (!cond) { table[i] = N; <fim do teste>; }`, onde `N` é o id estático do
site do assert. Fora de um teste, baixa para um panic.

### Test log intrinsic

`@log(msg)` aceita um `[]char` já formatado e acumula-o no buffer do teste
corrente. O buffer tem capacidade fixa por teste (proposta: 4 KiB) com
truncagem, para evitar alocação dinâmica sintetizada nesta iteração. A
formatação rica é construída em `std/test` sobre o `FormatBuffer` existente.

### Location table

O compilador grava uma tabela `(teste, id estático) -> ficheiro:linha:coluna`
na secção `Debug` do ZIRL, que hoje está reservada e vazia. O runner resolve o
slot lendo essa secção do artefacto que já tem em memória.

## Superfície de linguagem

- Novo valor `AttributeKind::Test` em `src/frontend/frontend.hpp`, aceite
  apenas no alvo `AttributeTarget::Function`.
- `classifyAttribute` em `src/frontend/attribute-classify.hpp` reconhece
  `test`. Os alvos ilegais continuam a usar `W2029`.
- `@assert` e `@log` entram na lista de nomes de intrínsecos reconhecidos pelo
  frontend e no caminho de sema/HIR como os intrínsecos de layout atuais.
- `#[test]` sobre uma função com parâmetros ou retorno não-void é `W2029`.
- Um módulo com `main` e `#[test]` em simultâneo é erro de sema.

## Superfície de config

- Documentar `test_dir` em `docs/19-project-config.md`, sob `[paths]`, com o
  valor por omissão `tests`.

## Protocolo de saída

- O driver imprime o output do programa e os logs durante a execução, e só no
  fim imprime um bloco delimitado com a tabela de resultados. Cada linha do
  bloco tem a forma `zith-test <nome> <slot>`, onde `<slot>` é `0` ou o id do
  assert falhado.
- O runner lê o último bloco delimitado e faz relay do resto. O delimitador
  tem de ser improvável de colidir com texto do teste. O texto de log é
  impresso pelo driver e nunca é interpretado pelo runner, por isso não há
  colisão de parsing.
- O exit code do comando é `0` quando todos os testes passam e `1` quando algum
  falha. A localização do assert falhado é recuperada pela secção `Debug`.

## Passos

1. Registar `AttributeKind::Test` e o reconhecimento de `test` em
   `classifyAttribute`, com `W2029` para alvos ilegais. Cobrir o caso de
   `#[test]` sobre função com parâmetros ou retorno não-void.

2. Adicionar `@assert(cond)` e `@log(msg)` como nomes de intrínsecos
   reconhecidos pelo frontend, com os tipos `bool` e `[]char`.

3. Baixar `@assert` e `@log` para HIR. Fora de teste, `@assert` baixa para um
   `HirRuntimePanic`. Dentro de teste, `@assert` baixa para escrita no slot e
   fim do teste, e `@log` baixa para um append no buffer do teste.

4. Sintetizar, por ficheiro de teste, a tabela de resultados, o buffer de log
   por teste e o `main` driver que chama as funções `#[test]` por ordem.

5. Gravar a tabela de localizações `(teste, id estático) -> localização` na
   secção `Debug` do ZIRL e implementar a leitura correspondente.

6. Reimplementar `test()` em `src/cli/cmd/tool.cpp` para compilar e executar
   cada ficheiro com o driver, capturar o output com o caminho `capture=true`,
   separar o bloco de resultados e imprimir o relatório por teste. Manter o
   `--filter <substring>` a casar contra o nome `modulo::fn`.

7. Reportar por omissão uma linha por teste, com o nome, e um resumo final.
   Com `--verbose`, mostrar o log de todos os testes e o detalhe extra.

8. Criar `std/test` com `assertEq`, `assertNe`, `assertTrue`, `assertFalse` e
   `fail` sobre `@assert`, usando o `FormatBuffer` existente para as mensagens.

9. Atualizar `docs/19-project-config.md` com `test_dir`, `docs/Zith--.md` com
   `#[test]`, `@assert` e `@log`, e a linha de estado em `docs/impl-status.md`.

10. Registar o desenho no ADR `docs/adr/0045-*`.

## Testes e critérios de sucesso

- Novo caso CTest que corre `zithc test` sobre a suite de exemplo em
  `tests/zith/`, com ficheiros que passam e ficheiros que falham de propósito,
  e verifica o exit code e as linhas do relatório.
- `zithc test` sobre um ficheiro com um assert falhado reporta o nome do teste
  e a localização correta do assert.
- `zithc test` sobre a suite toda passa nos três backends.
- `zithc test` sobre um ficheiro sem `#[test]` imprime a nota de salto e não
  falha.
- `#[test]` sobre função com parâmetros reporta `W2029`.
- `@assert(false)` fora de teste faz trap.
- A suite C++ e `fmt-check` continuam verdes.

## Fora de âmbito

- Isolamento por processo e um processo por teste. O MVP usa um driver por
  ficheiro.
- Captura de output por teste para os testes que passam. O `@log` cobre apenas
  o caso de mensagens explícitas.
- Crescimento dinâmico do buffer de log. O MVP usa capacidade fixa com
  truncagem.
- Formato TAP ou JUnit.
- Execução concorrente de ficheiros de teste. O comando mantém a ordem.
