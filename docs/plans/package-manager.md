# Gestor de Pacotes: Decisões do MVP e Plano de Implementação

Estado: desenho do MVP confirmado em 2026-10-29. Ainda não implementado.

Atualizado: 2026-10-29

## Objetivo

Este documento regista o desenho confirmado para consumir dependências Zith
locais em Zith--. Separa três questões que não devem ficar acopladas:

- Que pacotes um projeto seleciona, registados no lockfile do próprio projeto.
- Que formatos o distribuidor fornece: código-fonte, ZIRL ou ambos.
- Onde se reutilizam artefactos compilados: numa cache local ao projeto ou
  numa cache partilhada.

A cache não seleciona dependências. `resolve` fixa o grafo e `check`/`build`
usam essa seleção sem a alterar.

O contrato arquitetural está registado no
[ADR 0048](../adr/0048-local-package-resolution-and-artifact-cache.md).

## Estado Atual

- `docs/19-project-config.md` ainda documenta `[dependencies]` com
  `std = "bundled"`. O desenho confirmado trata `std` como parte da
  distribuição do compilador, não como dependência normal. A documentação de
  configuração será alinhada quando a implementação do gestor for integrada.
- `zithc deps list` lê valores string de `ZithProject.toml`.
  `deps add`, `deps remove`, `deps publish`, `deps unpublish` e `deps update`
  são stubs.
- `zithc create` cria `.zmodules` e escreve `paths.mod_dir = ".zmodules"`.
  O frontend não usa atualmente esse diretório como armazenamento de
  dependências.
- `CompilationSession::ensureFrontendContext()` acrescenta as raízes de
  inclusão do projeto e as raízes da biblioteca padrão descobertas pelo
  compilador a `FrontendConfig`. `FrontendContext::visibleRootsFor()` deriva
  as raízes usadas pela resolução de imports. Não existe uma etapa de resolução
  de pacotes que forneça raízes de dependências.
- O ADR 0002 define a semântica de imports independentemente da gestão de
  pacotes. A resolução de pacotes deve fornecer módulos sem alterar as regras
  de `import`, `from` ou `export`.
- A especificação já descreve `.zirl` como formato de cache e distribuição de
  bibliotecas compiladas. Na implementação atual, `zirl::Reader` descodifica um
  `cache::Artifact`, mas o carregamento passa pela store do cache de projeto,
  que valida a chave de cache, a impressão digital do código-fonte e as
  dependências do manifesto.
  Portanto, existe um formato de artefacto candidato, mas ainda falta confirmar
  e implementar o seu contrato de instalação portátil para o gestor de pacotes.
- A importação de headers C, a compilação de fontes C e a ligação nativa já
  existem como partes distintas do percurso FFI. `ffi.c_source_dirs`, os
  diretórios de inclusão, as bibliotecas e as defines são configurados
  separadamente. O desenho atual não transforma um projeto CMake num pacote
  Zith.
- O ADR 0006 propõe distribuir ferramentas de compilação nativa com `zithc`.
  Essa decisão diz respeito à distribuição do compilador, não ao local onde se
  instalam pacotes do utilizador.

## Modelo de Domínio

Usar estes termos de forma consistente:

- **Declaração de dependência**: pedido de um projeto para usar um pacote,
  identificado pelo nome e caminho local.
- **Pacote Zith**: unidade nomeada e versionada que fornece um ou mais módulos
  Zith e tem o seu próprio `ZithProject.toml`.
- **Dependência resolvida**: pacote e versão selecionados para um projeto e
  registados no respetivo lockfile.
- **Código-fonte do pacote**: fontes e metadados do pacote. Uma dependência de
  caminho local usa o conteúdo atual da fonte.
- **Artefacto de pacote**: ZIRL distribuído pelo publicador ou produzido ao
  compilar a fonte de um pacote.
- **Cache de pacotes**: armazenamento reutilizável de artefactos ZIRL. Pode
  ser local ao projeto ou partilhado, mas não seleciona versões nem substitui
  o lockfile.

## Decisões Confirmadas para o MVP

### Biblioteca Padrão

`std` vem com a distribuição do compilador e não aparece como dependência
normal no manifesto ou lockfile do projeto.

### Identidade e Instalação de Dependências

O MVP consome dependências por caminhos locais. Cada pacote tem nome, versão,
dependências e uma única raiz de módulos, `src/` por omissão, configurável no
manifesto. Caminhos relativos resolvem a partir do manifesto que os declara,
incluindo dependências transitivas.

### Sintaxe do Manifesto

O manifesto reutiliza `[project].name` e `[project].version` para a identidade
do pacote. Ambos são strings não vazias. A versão é uma identidade opaca no
MVP, sem interpretação de SemVer ou intervalos.

`[dependencies]` associa o nome declarado pelo pacote a um caminho local
relativo ao manifesto:

```toml
[dependencies]
logging = "../logging"
```

O nome da chave tem de corresponder a `[project].name` no manifesto do pacote
referenciado. A declaração não contém alias nem requisito de versão. O caminho
é interpretado relativamente ao manifesto que declara a dependência.

`[package].module_root` define a única raiz de módulos do pacote, relativamente
ao seu manifesto. O campo é opcional e tem por omissão `src`:

```toml
[package]
module_root = "modules"
```

Não se reutiliza `[paths].src_dir`, que continua a configurar as raízes de
origem do projeto consumidor e pode conter vários diretórios.

O projeto mantém a sua própria seleção no lockfile, que deve ser incluído no
controlo de versões. O lockfile regista o grafo, os nomes, versões e caminhos
resolvidos e as impressões digitais do manifesto do projeto e de cada pacote
transitivo. Não fixa o conteúdo dos ficheiros fonte locais. Alterações à fonte
são vivas. Alterações a qualquer manifesto tornam o lockfile desatualizado.

`resolve` preserva seleções ainda válidas. Intervalos de versão, aliases,
coexistência de versões e `update` ficam fora do MVP. O manifesto identifica a
dependência pelo nome declarado pelo pacote. `deps add` e `deps remove`
alteram apenas o manifesto, e o utilizador chama `deps resolve` explicitamente.

### Lockfile e comandos de compilação

`check` e `build` exigem um lockfile válido. Se faltar ou estiver desatualizado,
falham com uma indicação para executar `zithc deps resolve`. Não resolvem o
grafo, não atualizam o lockfile e não obtêm pacotes remotamente. Validam os
impressões digitais dos manifestos de todo o grafo antes de preparar as
dependências.

Depois de validar o lockfile, `check` e `build` podem preparar a cache para as
dependências selecionadas. Se não houver um ZIRL compatível, compilam a fonte
disponível e guardam o ZIRL na cache, sem alterar o lockfile. Se não houver
fonte utilizável, reportam um erro explícito.

### Distribuição e Compilação

O distribuidor escolhe se publica ZIRL, fonte ou ambos. ZIRL é a opção por
omissão e a fonte só é partilhada quando o distribuidor a inclui.

| Distribuição | Comportamento |
|---|---|
| ZIRL compatível | Usar o artefacto. |
| Sem ZIRL compatível, com fonte | Compilar a fonte e guardar o resultado na cache. |
| Sem ZIRL compatível nem fonte | Falhar com um diagnóstico explícito. |

A identidade da cache inclui a identidade e a versão do pacote, a impressão
digital da fonte ou do ZIRL distribuído que originou o artefacto e os metadados
de compatibilidade relevantes, como formato ZIRL, compilador, alvo e opções
aplicáveis. Alterações à fonte produzem uma entrada de cache distinta sem
exigir novo `resolve`. A escolha entre cache local ao projeto e cache
partilhada é configurável por projeto. A cache partilhada reutiliza artefactos,
não copia pacotes para cada projeto e não muda a seleção do lockfile.

Não executar scripts personalizados fornecidos por pacotes.

### Imports e grafo de dependências

Os módulos dos pacotes usam os caminhos de importação existentes, sem prefixo
automático com o nome do pacote. A resolução preserva as regras existentes de
`import`, `from` e `export`.

O compilador resolve as dependências transitivas para cada pacote, mas o projeto
consumidor só vê as dependências que declarou diretamente. Um pacote pode
expor módulos de uma dependência transitiva através de `export`. O MVP rejeita
ciclos de pacotes, caminhos de módulo fornecidos por mais de um pacote e
declarações do mesmo nome de pacote vindas de diretórios distintos. Os
diagnósticos de conflito identificam os caminhos envolvidos.

### Pacotes C

Pacotes C, adaptadores de build e integração com CMake ficam fora do MVP.
Qualquer suporte futuro será uma decisão separada e não permitirá execução
implícita de scripts arbitrários.

## Fora do Âmbito do MVP

- Referências Git, registry, autenticação, publicação e proveniência remota.
- Intervalos de versão, aliases, comando `update` e coexistência de versões.
  A coexistência continua uma direção possível para versões futuras de Zith,
  mas não para Zith-- inicial.
- Pacotes C, adaptadores de build, CMake e execução de scripts de pacote.
- O formato versionado exato do lockfile.
- O conjunto exato de metadados que define compatibilidade de ZIRL, incluindo
  compilador, ABI, alvo e opções de build aplicáveis.
- Política de retenção e limpeza da cache.

Estas questões não bloqueiam o modelo do MVP. Devem ser decididas antes de
implementar as áreas correspondentes, sem alargar implicitamente o âmbito.

## Plano de Implementação

### Passo 1 - Modelar e Interpretar Declarações de Pacotes

Objetivo: `ZithProject.toml` representa pacotes locais com identidade,
dependências e uma única raiz de módulos configurável.

Subpassos:

1. Alargar `/home/diogo/Zith/src/cli/project-config.hpp` com dependências
   estruturadas, nome do pacote, versão declarada, caminho e raiz de módulos.
2. Resolver caminhos relativos ao manifesto que declara cada dependência.
3. Usar `src/` como raiz de módulos por omissão. Rejeitar aliases e intervalos
   de versão no MVP.
4. Atualizar a leitura TOML em
   `/home/diogo/Zith/src/session/compilation-session.cpp` e produzir
   diagnósticos associados ao campo inválido.
5. Acrescentar testes para declarações válidas, campos ausentes, tipos
   inválidos, nomes/versões inconsistentes e caminhos relativos transitivos.

Critério de sucesso: os manifestos de consumidor e pacote descrevem as
dependências sem iniciar resolução ou alterar ficheiros.

### Passo 2 - Resolver e Fixar o Grafo

Objetivo: `zithc deps resolve` calcula o grafo local determinístico e grava as
identidades exatas no lockfile.

Subpassos:

1. Criar `/home/diogo/Zith/src/packages/` para validação, resolução local,
   lockfile e acesso à cache.
2. Fazer `deps add` e `deps remove` alterarem apenas o manifesto. Implementar
   `deps resolve` como ação explícita e manter `deps update` fora do MVP.
3. Gravar o grafo transitivo, os nomes, versões declaradas e caminhos
   normalizados, bem como impressões digitais dos manifestos do projeto e de
   cada pacote. Incluir o lockfile no fluxo normal de controlo de versões.
4. Preservar a resolução existente enquanto continuar válida. Rejeitar ciclos,
   o mesmo nome de pacote vindo de diretórios distintos e módulos com caminhos
   duplicados, identificando os caminhos envolvidos.
5. Fazer `check` e `build` falharem se o lockfile faltar ou se os metadados de
   qualquer pacote o tornarem desatualizado. Apresentar `zithc deps resolve`
   como ação corretiva.
6. Testar lockfile determinístico, resolução repetida, origem local inexistente,
   manifesto/lockfile malformado, conflito de identidade e ciclos.

Comandos de verificação:

```sh
cmake --build /home/diogo/Zith/build --target test-packages test-cli-commands -j 4
ctest --test-dir /home/diogo/Zith/build -R 'package|cli-commands' --output-on-failure
```

Critério de sucesso: inputs de manifesto inalterados produzem sempre o mesmo
lockfile, e `check`/`build` nunca o alteram implicitamente.

### Passo 3 - Integrar Dependências com Imports

Objetivo: módulos selecionados pelo lockfile ficam visíveis sem alterar a
semântica existente de imports.

Subpassos:

1. Acrescentar ao `FrontendConfig` as raízes dos pacotes diretamente
   selecionados pelo lockfile, usando a raiz configurada de cada pacote.
2. Permitir que cada pacote resolva as suas dependências transitivas no seu
   próprio contexto. Não expor essas dependências diretamente ao projeto
   consumidor, salvo através de `export`.
3. Preservar os caminhos de módulo existentes, sem prefixo automático do nome
   do pacote, e rejeitar ambiguidades de caminhos.
4. Incluir a identidade resolvida e a impressão digital relevante da fonte na
   identidade do cache frontend.
5. Testar import direto, dependência transitiva privada, reexportação, pacote
   em falta, raízes duplicadas e invalidação após alteração de identidade.

Critério de sucesso: os imports de pacotes seguem as mesmas regras de
`import`, `from` e `export` dos módulos do projeto.

### Passo 4 - Preparar e Reutilizar Artefactos ZIRL

Objetivo: `check` e `build` reutilizam ZIRL compatível ou compilam a fonte
disponível para a cache, sem mudar a seleção do projeto.

Subpassos:

1. Implementar cache local ao projeto e cache partilhada como âmbitos
   alternativos configuráveis por projeto. A seleção continua sempre no
   lockfile.
2. Procurar primeiro um ZIRL compatível. Se estiver ausente ou incompatível e
   houver fonte, compilar essa fonte e guardar o resultado na cache.
3. Se não houver ZIRL compatível nem fonte, falhar com um diagnóstico explícito.
4. Distinguir entradas de cache pela identidade/versão do pacote, impressão
   digital da fonte ou do ZIRL distribuído e metadados de compatibilidade
   acordados para o artefacto.
5. Confirmar que alterações apenas à fonte local não exigem `resolve`, mas
   nunca reutilizam ZIRL derivado de conteúdo diferente. Alterações ao manifesto
   do pacote devem invalidar o lockfile.
6. Testar acerto de cache, conteúdo fonte alterado, artefacto incompatível com
   e sem fonte, e `check`/`build` sem alteração do lockfile.

Critério de sucesso: a cache só evita trabalho de compilação. Remover ou
partilhar a cache não altera o grafo resolvido nem os módulos visíveis.

### Passo 5 - Verificação Final do MVP

Antes de declarar o gestor de pacotes pronto, verificar:

1. `std` vem com o compilador e não é uma dependência normal.
2. O MVP aceita apenas caminhos locais e pacotes com versão declarada.
3. Dependências transitivas, ciclos e conflitos seguem as regras deste
   documento.
4. O lockfile é explícito, versionado no projeto e obrigatório para
   `check`/`build`.
5. `check`/`build` podem preencher a cache, mas não resolvem nem alteram o
   lockfile.
6. Cache local e partilhada reutilizam artefactos sem controlar a seleção do
   projeto.
7. ZIRL incompatível usa fonte disponível ou produz erro explícito.
8. Imports existentes mantêm a semântica definida no ADR 0002.
9. A implementação não executa scripts personalizados de pacotes.
10. Passam os testes de pacotes, CLI e frontend.
