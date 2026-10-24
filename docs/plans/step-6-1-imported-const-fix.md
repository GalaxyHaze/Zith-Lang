# Step 6.1: Correção de constantes importadas

**Estado:** Implementado e validado; issues registados (mapa #74, correção
principal #75, follow-up de codegen #76). Falha remanescente na suite é
preexistente e não relacionada.

**Objetivo:** Preservar `BindingKind::Const` nos imports do frontend Zith-- e
corrigir a classificação semântica de constantes importadas, para que estas
sejam baixadas para `HirGlobalConstLoad` e deixem de bloquear o fixture
`zirl-hir-artifact` de self-Zith.

**Pré-condições:** Trabalhar a partir de `/home/diogo/self-Zith`. O utilizador
autorizou as alterações pontuais de código e testes em `/home/diogo/Zith`;
esta autorização não inclui uma refatoração do frontend nem a correção
separada do codegen. O worktree sibling está sujo. Antes de cada operação
nesse repositório, executar
`git -C /home/diogo/Zith status --short --branch` e inspecionar os diffs
relevantes. Preservar todas as alterações preexistentes, incluindo o teste de
cache genérico não relacionado em
`/home/diogo/Zith/tests/test-hir-lower-modern.cpp`. O worktree self-Zith tem
uma alteração preexistente em `src/cache/zirl-buffer.zith`; não tocar nesse
ficheiro. Builds, testes e escritas em `/home/diogo/Zith` poderão exigir
aprovação adicional da sandbox. Não fazer staging, commit ou push nesta
tarefa.

**Ficheiros a alterar durante a implementação:**

- `/home/diogo/Zith/src/session/frontend-context.hpp`
- `/home/diogo/Zith/src/session/frontend-module-analysis.cpp`
- `/home/diogo/Zith/src/session/frontend-symbol-resolution.cpp`
- `/home/diogo/Zith/src/sema/sema-zith.cpp`
- `/home/diogo/Zith/src/sema/hir-lower-expr-agg.cpp`
- `/home/diogo/Zith/tests/test-hir-lower-modern.cpp`
- `/home/diogo/self-Zith/docs/plans/shared-hir-interoperability-status.md`

`hir-lower-expr-agg.cpp` não estava na lista original: a terceira regressão
(acesso qualificado `ns.VALUE`) mostrou que `lowerField()` devolvia
`kInvalidHirExpr` para um membro importado que é constante, porque o ramo
`ResolutionKind::Import` só tratava funções. O caminho de
`HirGlobalConstLoad` foi acrescentado aí, em paralelo com `lowerName()`.

Não alterar ficheiros do codegen. Registar a omissão silenciosa de stores como
um follow-up independente.

## 1. Revalidar os worktrees e registar os issues

**Objetivo:** Confirmar que o trabalho local preexistente foi identificado e
que o tracker contém um mapa, um issue principal e um follow-up independente.

**Ações:** Executar os comandos abaixo. Confirmar que o diff preexistente em
`test-hir-lower-modern.cpp` contém apenas
`test_generic_template_survives_persistent_cache`; preservá-lo. Se qualquer
ficheiro de produção listado acima já tiver alterações, parar antes de o
editar e mostrar o diff. Criar um issue `wayfinder:map`, depois um issue
`bug`/`ready-for-agent` para a correção principal e um issue `bug`/
`needs-triage` para o codegen que omite stores quando `emitVar` devolve
`nullptr`. Referenciar os dois filhos no mapa e ligar relações nativas de
sub-issue quando disponíveis; caso contrário, incluir `Map: #N` no corpo de
cada filho.

```sh
git -C /home/diogo/Zith status --short --branch
git -C /home/diogo/Zith diff -- \
  src/session/frontend-context.hpp \
  src/session/frontend-module-analysis.cpp \
  src/session/frontend-symbol-resolution.cpp \
  src/sema/sema-zith.cpp \
  tests/test-hir-lower-modern.cpp
git -C /home/diogo/self-Zith status --short
```

Usar `gh issue create --repo GalaxyHaze/Zith-Lang` com estes títulos:
`Step 6.1: imported const resolution blocks typed HIR writer` para o mapa,
`Preserve const binding metadata across Zith-- imports` para o issue
principal e `Codegen: report failed variable emission instead of omitting
stores` para o follow-up.

**Saída esperada:** Os estados mostram apenas as alterações locais conhecidas;
o diff do teste contém apenas o teste de cache genérico não relacionado; o
GitHub devolve URL/número para os três issues e o mapa referencia os dois
filhos.

**Verificações de falha:** Se um ficheiro de produção-alvo já estiver
modificado, parar e pedir orientação. Se o GitHub devolver erro de rede ou de
label, guardar a saída exata, não afirmar que os issues foram registados e
parar antes da implementação.

**Critério de sucesso:** Os diffs-alvo foram revistos, as alterações
preexistentes estão identificadas para preservação e os três issues aparecem
em `gh issue list --repo GalaxyHaze/Zith-Lang`.

## 2. Acrescentar e corrigir a regressão de import seletivo

**Objetivo:** Uma constante importada seletivamente é aceite pela sema e
baixada como load de uma constante global.

**Ações:** Em `tests/test-hir-lower-modern.cpp`, acrescentar
`test_selective_imported_const_lowers_to_global_const_load()` junto de
`test_const_global_lowers_to_load()` e registá-lo junto dos testes de
constantes no runner `test_hir_lower_modern()`. Usar estes fixtures:

```text
dep.zith:  pub const VALUE: i32 = 7;
main.zith:
  from dep { VALUE as selected }
  const COPY: i32 = selected;
  fn main(): i32 { return selected; }
```

Verificar que `runTo(Stage::HirLowered)` tem sucesso, que o HIR contém as
constantes globais esperadas e que pelo menos uma expressão é
`HirGlobalConstLoad`. Compilar e executar o teste antes de alterar produção;
confirmar a falha de regressão. Depois, acrescentar `frontend::BindingKind
bindingKind` a `LocalSymbolInfo` em `frontend-context.hpp`, atribuir
`declaration.bindingKind` ao construir `LocalSymbolInfo` em
`frontend-module-analysis.cpp` e copiar `symbol.bindingKind` para o
`ResolvedName` do caminho de import seletivo em
`frontend-symbol-resolution.cpp`. Em `PerModuleSema::isConstantExpression()`
de `sema-zith.cpp`, classificar o import por `resolved->declKind` e
`resolved->bindingKind`; remover o acesso que usa o ID do símbolo fornecedor
como índice das declarações do módulo consumidor.

```sh
cmake --build /home/diogo/Zith/build --target test-hir-lower-modern -j2
ctest --test-dir /home/diogo/Zith/build \
  -R '^test-hir-lower-modern$' --output-on-failure
```

Repetir os mesmos comandos depois da alteração de produção.

**Saída esperada:** Antes da correção, a nova regressão falha; depois, o alvo
mostra `100% tests passed`.

**Verificações de falha:** Se o teste não compilar, corrigir apenas o fixture
ou o uso da API de teste. Se falhar por motivo alheio à classificação/lowering
da constante importada, parar e reportar o diagnóstico. Se ainda houver erro
de sema após a correção, inspecionar o `ResolvedName` do import; não voltar a
indexar o snapshot consumidor pelo ID do fornecedor.

**Critério de sucesso:** O novo teste seletivo passa e confirma um
`HirGlobalConstLoad`; o teste preexistente de cache genérico continua presente
e inalterado.

## 3. Acrescentar e corrigir a injeção de `from` sem seletor

**Objetivo:** `from dep` sem seletores preserva o tipo de binding da constante
importada.

**Ações:** Acrescentar
`test_from_imported_const_lowers_to_global_const_load()` em
`tests/test-hir-lower-modern.cpp`, usando `pub const VALUE: i32 = 7;` em
`dep.zith` e `from dep` seguido de `return VALUE` em `main.zith`. Confirmar
que o HIR contém um `HirGlobalConstLoad`. Executar o teste para reproduzir a
falha de propagação. Depois, copiar `symbol.bindingKind` para o `ResolvedName`
no ramo `edge.request.isFrom || edge.request.isExport` de
`frontend-symbol-resolution.cpp`. Registar o teste junto da regressão
seletiva e voltar a executar o alvo HIR.

**Saída esperada:** O teste reproduz a ausência do global-constant load antes
da alteração e passa depois.

**Verificações de falha:** Se `from dep` não resolver o fixture, comparar a
sintaxe com os testes de imports existentes e corrigir apenas o fixture. Se
falhar após a propagação, inspecionar o `ResolvedName` na resolução de módulos
e parar caso o alvo ou o `declKind` estejam inesperados.

**Critério de sucesso:** Os testes de import seletivo e de `from` sem seletor
passam em `test-hir-lower-modern`.

## 4. Acrescentar e corrigir o acesso qualificado

**Objetivo:** `namespace.VALUE` preserva a semântica de constante e os quatro
pontos de cópia propagam `bindingKind`.

**Ações:** Acrescentar
`test_qualified_imported_const_lowers_to_global_const_load()` usando
`import dep as ns` e `return ns.VALUE`; afirmar lowering bem-sucedido e
presença de `HirGlobalConstLoad`. Confirmar a regressão antes da alteração.
Depois, copiar `symbol.bindingKind` na resolução de `Field` qualificado em
`frontend-symbol-resolution.cpp` e também no caminho de struct literal
qualificado, para manter a transferência de metadados completa. Registar o
teste junto dos outros testes de constantes importadas.

Auditar os quatro caminhos no ficheiro de resolução: seletor, injeção
`from`/`export`, struct literal qualificado e cadeia de campos qualificada.
Executar:

```sh
rg -n -C 3 'declKind\s*=\s*symbol.kind' \
  /home/diogo/Zith/src/session/frontend-symbol-resolution.cpp
```

**Saída esperada:** O teste qualificado passa e cada uma das quatro cópias
tem uma atribuição correspondente a `bindingKind`.

**Verificações de falha:** Se `ns.VALUE` não resolver, parar e registar a
entrada de resolução e o diagnóstico. Não alargar a arquitetura de imports.
Não criar teste dedicado para struct literal: não é um caminho de acesso a
constantes e será coberto pela auditoria explícita dos quatro pontos.

**Critério de sucesso:** As três formas de import passam e a auditoria mostra
os quatro pontos previstos a propagarem `bindingKind`.

## 5. Executar a sequência de validação acordada

**Objetivo:** Os testes C++ focados, o harness original de self-Zith e a suite
CTest completa passam nessa ordem.

**Ações:** Compilar o compilador e os quatro alvos focados. Executar primeiro
os alvos CTest focados, depois o harness original e, por fim, toda a suite
CTest. Não alterar a validação do cache self-Zith para fazer o harness passar.

```sh
cmake --build /home/diogo/Zith/build --target \
  zithc test-frontend-context test-frontend-modern-pipeline \
  test-hir-lower-modern test-codegen -j2
ctest --test-dir /home/diogo/Zith/build \
  -R '^(test-frontend-context|test-frontend-modern-pipeline|test-hir-lower-modern|test-codegen)$' \
  --output-on-failure
ZITHC=/home/diogo/Zith/build/zithc TMPDIR=/tmp \
  sh /home/diogo/self-Zith/tests/run.sh
ctest --test-dir /home/diogo/Zith/build --output-on-failure
```

**Saída esperada:** Todos os comandos devolvem `0`; em particular, o fixture
`zirl-hir-artifact` deixa de devolver `InvalidSections`.

**Verificações de falha:** Se os testes focados falharem, parar antes do
harness e corrigir apenas o caminho de import que falhou. Se os focados
passarem mas o harness continuar a devolver `2`, guardar o fixture e a saída
exatos; não enfraquecer a validação de secções nem omitir uma secção ZIRL
obrigatória. Se a suite completa falhar num teste preexistente não relacionado,
preservar a saída e separar essa falha da correção.

**Critério de sucesso:** CTest focado, comando exato do harness e CTest
completo terminam todos com código `0`.

## 6. Registar o resultado e fechar apenas o issue concluído

**Objetivo:** O status documenta a causa confirmada e os resultados reais; o
follow-up de codegen permanece aberto.

**Ações:** Atualizar
`/home/diogo/self-Zith/docs/plans/shared-hir-interoperability-status.md` para
substituir a hipótese provisória sobre codecs/envelope pela causa confirmada
no frontend Zith--. Registar se a correção e cada comando de validação
passaram. Comentar e fechar o issue principal apenas depois de todos os
critérios do passo 5 passarem; atualizar o mapa com a resolução. Manter aberto
o follow-up do codegen. Preservar todo o trabalho preexistente. Não fazer
staging, commit ou push.

```sh
git -C /home/diogo/Zith diff --check -- \
  src/session/frontend-context.hpp \
  src/session/frontend-module-analysis.cpp \
  src/session/frontend-symbol-resolution.cpp \
  src/sema/sema-zith.cpp \
  tests/test-hir-lower-modern.cpp
git -C /home/diogo/Zith status --short
git -C /home/diogo/self-Zith status --short
```

**Saída esperada:** O diff check dos ficheiros-alvo está limpo; o status
documenta a correção e os testes; o issue principal está fechado e o follow-up
permanece aberto.

**Verificações de falha:** Se o diff check apontar whitespace numa alteração
alvo, corrigir apenas essa linha e repetir a verificação. Se o status mostrar
que um ficheiro preexistente desapareceu ou mudou inesperadamente, parar sem
restaurar nada e reportar o caminho.

**Critério de sucesso:** Causa, correção, validação e follow-up separado estão
registados sem perturbar alterações não relacionadas.

## Aceitação final

Aceitar Step 6.1 apenas quando as regressões seletiva, `from` sem seletor e
namespace-qualified passarem; os quatro pontos de cópia estiverem auditados;
o harness original e o CTest completo devolverem `0`; e o status e o mapa
registarem o resultado. O contrato de validação do envelope permanece
inalterado.
