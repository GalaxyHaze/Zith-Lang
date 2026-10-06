# Gestor de Pacotes: Revisão de Decisões e Proposta de Implementação

Estado: proposta para discussão. Este documento não aprova um ADR nem altera a
implementação atual.

Atualizado: 2026-10-05

## Objetivo

Este documento revê as decisões atuais relacionadas com pacotes e separa três
questões que não devem ficar acopladas:

- Onde se instala uma dependência: no projeto ou num armazenamento associado
  ao compilador.
- O que distribui um publicador: código-fonte, artefactos compilados ou ambos.
- Como se constrói uma dependência: pelo compilador Zith ou por um adaptador
  específico da linguagem, como CMake.

O objetivo é clarificar o modelo de pacotes antes de implementar `zithc deps`.

## Estado Atual

- `docs/19-project-config.md` documenta `[dependencies]` com
  `std = "bundled"`. O utilizador propõe remover esta entrada. É documentação,
  não uma dependência implementada da biblioteca padrão.
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
  que valida cache key, fingerprint de source e dependências do manifesto.
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

Usar estes termos de forma consistente durante o desenho:

- **Declaração de dependência**: pedido de um projeto para usar um pacote,
  incluindo o nome e os requisitos de origem/versão usados na resolução.
- **Dependência resolvida**: identidade exata do pacote selecionado para um
  projeto e registada no lockfile.
- **Código-fonte do pacote**: ficheiros fonte e metadados publicados.
- **Artefacto de pacote**: resultado compilado, versionado, que pode ser
  distribuído em vez do código-fonte ou juntamente com ele. Não chamar a isto
  cache do compilador.
- **Instalação no projeto**: cópia de trabalho ou materialização de um pacote associado
  a um projeto.
- **Armazenamento global**: armazenamento reutilizável de pacotes, associado a
  uma instalação de `zithc`. Um pacote neste armazenamento não pode alterar
  silenciosamente a versão selecionada pelo lockfile de um projeto.
- **Adaptador de build**: implementação que transforma os ficheiros declarados
  por um pacote em inputs ou artefactos do compilador. O resolver de pacotes
  não deve conter regras específicas de Zith ou C.

O âmbito de instalação, o formato de distribuição e o adaptador de build são
dimensões independentes. Por exemplo, um pacote Zith de código-fonte instalado
no projeto e um artefacto C compilado instalado globalmente são combinações
válidas.

## Direção Proposta

Os pontos seguintes são recomendações para revisão, não decisões aceites.

### Biblioteca Padrão

Remover `std = "bundled"` do exemplo de dependências. Tratar a biblioteca
padrão como parte versionada da distribuição do compilador, descoberta pelo
compilador como atualmente. Um projeto não deve ter de adicionar ou instalar
`std` como pacote normal.

Antes de alterar a especificação, atualizar todos os exemplos da configuração
do projeto, incluindo `docs/19-project-config.md` e
`docs/Zith-spec-full.md`. Manter a versão e os metadados de compatibilidade da
biblioteca padrão separados do lockfile de dependências do projeto.

### Identidade e Instalação de Dependências

Manter a declaração no manifesto como intenção do projeto e usar um lockfile
para as identidades exatas resolvidas. O armazenamento global deve ser uma
otimização e um âmbito de instalação explícito, não uma regra implícita de
seleção de versões.

Tratar `.zmodules` como diretório inicial de materialização do projeto apenas
se esse papel for confirmado. Não tratar o campo existente `paths.mod_dir` como
decisão aceite sobre o armazenamento de pacotes. Preservar a opção de usar
internamente um armazenamento endereçado por conteúdo e expor raízes de pacote
estáveis ao frontend.

Não assumir que o diretório ao lado do compilador permite escrita. O desenho
do armazenamento global precisa de um fallback documentado para instalações do
compilador só de leitura e de considerar várias versões do compilador. Uma
área de armazenamento por utilizador, indexada pela compatibilidade do
compilador, reduz o risco de problemas de permissões comparada com a exigência
de escrita administrativa.

Inicialmente, o gestor de pacotes deve suportar código-fonte local ou caminhos
antes de exigir um registry. A escolha de registry, autenticação e publicação
pode ser feita depois de estabilizar a identidade dos pacotes e o lockfile.

### Distribuição e Compilação

Usar pacotes de código-fonte como formato canónico inicial. Permitir que um
pacote inclua mais tarde um artefacto compilado opcional:

| Distribuição | Comportamento |
|---|---|
| Código-fonte | Compilar com uma versão compatível do compilador instalado. |
| Só artefacto | Usar apenas um artefacto compatível. Se não existir, apresentar um erro acionável. |
| Código-fonte e artefacto | Usar um artefacto compatível; caso contrário, compilar o código-fonte incluído. |

Definir explicitamente a compatibilidade do artefacto antes de implementar a
distribuição só com artefactos. Avaliar, no mínimo, a identidade do conteúdo do
pacote, a versão do compilador ou ABI da linguagem, o target, as opções de
build e as identidades das dependências. Não reutilizar o cache de projeto
atual como formato de intercâmbio sem uma decisão independente sobre
compatibilidade e portabilidade.

### Pacotes C

Manter o manifesto e o resolver independentes da linguagem. Os adaptadores de
build descrevem se um pacote fornece módulos Zith, headers C, código-fonte C ou
bibliotecas nativas pré-compiladas.

Na primeira integração de C, mapear os metadados do pacote para os diretórios
de inclusão, diretórios de código-fonte C, defines, diretórios de bibliotecas e
inputs de ligação existentes. Isto reutiliza o percurso FFI sem tornar CMake
obrigatório.

Considerar CMake como adaptador de build C opcional para pacotes que precisem
de um grafo de build nativo, geração de código ou configuração específica da
plataforma. O resolver deve passar-lhe um diretório de pacote, target e
contexto de toolchain explícitos. Não exigir CMake para todas as dependências
nem executar scripts de build arbitrários só porque um pacote foi instalado.

## Decisões em Aberto

Resolver estas questões antes de fixar o formato do manifesto ou lockfile:

- "Instalação global" significa um armazenamento ao lado de cada distribuição
  do compilador, um armazenamento por utilizador ou ambos com precedência
  definida?
- Quais são as origens de pacote da primeira versão: caminhos locais,
  referências Git, um registry oficial ou um subconjunto?
- Que sintaxe de requisitos de versão é aceite e que dados exatos fixa o
  lockfile?
- A primeira versão suporta dependências transitivas? A recomendação é
  suportá-las com resolução determinística e lockfile.
- Um projeto pode substituir uma dependência fixada por uma cópia local?
  Como se declara explicitamente essa substituição?
- Que diferenças de compilador, target, ABI e opções invalidam um artefacto
  compilado?
- Pode publicar-se um pacote só com artefacto? Qual é a janela de
  compatibilidade prometida?
- O primeiro adaptador de pacotes C suporta apenas os inputs `ffi` atuais ou
  também executa CMake?
- Que versões CMake, generators, toolchains e outputs gerados suportaria um
  adaptador CMake opcional?
- Como se representam licenças, hashes, proveniência e etapas de build não
  fidedignas?

## Plano de Implementação

### Passo 1 - Aceitar o Vocabulário e o Âmbito do Manifesto

Objetivo: acordar os conceitos de pacote e o âmbito da primeira versão antes de
o código depender deles.

Subpassos:

1. Rever os termos em "Modelo de Domínio" e confirmar que identidade do pacote,
   âmbito de instalação, formato de distribuição e adaptador de build continuam
   separados.
2. Decidir a primeira origem de pacote suportada e o papel do lockfile.
3. Decidir se `.zmodules` é um diretório de materialização do projeto ou se
   deve ser substituído por outro caminho configurado.
4. Registar como ADR apenas as decisões difíceis de reverter. Manter as opções
   em aberto neste plano.

Ficheiros e alterações:

- Se aprovado, registar a terminologia de pacotes em
  `/home/diogo/Zith/CONTEXT.md`.
- Se aprovado, criar um registo de decisão específico em
  `/home/diogo/Zith/docs/adr/`.
- Não editar `docs/19-project-config.md` nem outros ficheiros já modificados
  antes de rever as alterações existentes com o utilizador.

Resultado esperado: terminologia aceite e lista escrita das origens,
âmbitos de instalação e garantias do lockfile da primeira versão.

Verificações de falha:

- Se origem, versão ou âmbito de instalação continuar a ter mais de uma
  interpretação, manter a sintaxe do manifesto em aberto e não implementar o
  parser.
- Se o ADR proposto entrar em conflito com outro ADR aceite, parar e resolver
  o conflito antes de alterar a implementação.

Critério de sucesso: cada pacote da primeira versão tem uma identidade
determinística representável no manifesto e no lockfile.

### Passo 2 - Modelar e Interpretar Declarações de Pacotes

Objetivo: `ZithProject.toml` representa dependências como declarações
estruturadas, sem as resolver ou instalar durante a interpretação do ficheiro.

Subpassos:

1. Alargar `/home/diogo/Zith/src/cli/project-config.hpp` com uma representação
   de dependência capaz de guardar o nome do pacote e os campos aceites no
   Passo 1.
2. Atualizar a leitura de TOML em
   `/home/diogo/Zith/src/session/compilation-session.cpp`, onde atualmente se
   carregam os campos `[ffi]` e `[project]`, para interpretar a representação
   acordada.
3. Associar erros de validação ao campo do manifesto e rejeitar entradas
   inválidas em vez de as ignorar silenciosamente.
4. Acrescentar casos válidos, inválidos e com campos em falta em
   `/home/diogo/Zith/tests/test-cli-commands.cpp` ou num ficheiro dedicado
   `/home/diogo/Zith/tests/test-packages.cpp`.

Comando exato de build:

```sh
cmake --build /home/diogo/Zith/build --target test-cli-commands -j 4
```

Resultado esperado: o target compila e os testes focados na configuração
passam.

Verificações de falha:

- Se um teste não distinguir um campo em falta de um tipo inválido, criar um
  fixture separado para cada caso.
- Se o parser TOML aceitar uma forma de manifesto não suportada, rejeitá-la na
  validação da configuração do projeto antes de continuar.

Critério de sucesso: valores de dependência estruturados passam os testes da
configuração sem provocar operações de rede ou de instalação.

### Passo 3 - Resolver e Fixar Dependências

Objetivo: um comando explícito resolve o conjunto de pacotes declarado e regista
as identidades exatas de forma reproduzível.

Subpassos:

1. Criar um módulo de gestão de pacotes em
   `/home/diogo/Zith/src/packages/` para validação de declarações, resolução de
   origens, lockfile e metadados de instalação.
2. Manter o armazenamento de ficheiros e os fornecedores de origem atrás de
   interfaces internas, para os testes usarem diretórios temporários e origens
   locais sem rede.
3. Implementar o primeiro fornecedor de origem escolhido no Passo 1. Não
   acrescentar registry, Git ou autenticação sem decisão explícita.
4. Substituir os stubs `deps add` e `deps remove` em
   `/home/diogo/Zith/src/cli/cmd/tool.cpp` por comandos que atualizem o
   manifesto com segurança. Implementar um comando separado de resolução ou
   instalação se o fluxo acordado o exigir.
5. Criar `/home/diogo/Zith/tests/test-packages.cpp` e registá-lo junto dos
   testes existentes em `/home/diogo/Zith/CMakeLists.txt`.
6. Testar lockfile estável, resolução repetida, origens em falta, lockfiles
   malformados e ciclos de dependências.

Comandos exatos de verificação:

```sh
cmake --build /home/diogo/Zith/build --target test-packages test-cli-commands -j 4
ctest --test-dir /home/diogo/Zith/build -R 'package|cli-commands' --output-on-failure
```

Resultado esperado: os dois executáveis de teste compilam e o CTest não reporta
falhas nos testes selecionados.

Verificações de falha:

- Se resoluções repetidas alterarem o lockfile sem alterações nos inputs, parar
  e corrigir a ordenação ou derivação de identidade antes da integração com a
  compilação.
- Se a instalação puder substituir parcialmente um pacote existente, tornar a
  atualização atómica e acrescentar um teste de interrupção/falha.

Critério de sucesso: duas resoluções limpas do mesmo manifesto e conjunto de
ficheiros de código-fonte produzem lockfiles e identidades de pacote idênticos.

### Passo 4 - Ligar Pacotes Zith aos Imports

Objetivo: dependências Zith fixadas ficam visíveis ao frontend sem alterar a
semântica de imports.

Subpassos:

1. Acrescentar raízes de dependências resolvidas e identidades estáveis à
   configuração construída por `CompilationSession::ensureFrontendContext()`
   em `/home/diogo/Zith/src/session/compilation-session.cpp`.
2. Atualizar `FrontendContext::visibleRootsFor()` em
   `/home/diogo/Zith/src/session/frontend-module-analysis.cpp` para incluir
   apenas as raízes instaladas e selecionadas pelo resolver.
3. Preservar a precedência de raízes decidida no Passo 1 e reportar
   ambiguidades em vez de depender da ordem de iteração do sistema de ficheiros.
4. Incluir as identidades das dependências resolvidas na identidade do cache
   frontend, para uma alteração de dependência não reutilizar um artefacto
   incompatível.
5. Acrescentar testes a
   `/home/diogo/Zith/tests/test-frontend-context.cpp` para um pacote resolvido,
   um pacote em falta, raízes de módulos duplicadas e invalidação após alteração
   da identidade de uma dependência.
6. Verificar que o comportamento de imports em
   `/home/diogo/Zith/docs/adr/0002-modern-module-import-semantics.md` não muda.

Comandos exatos de verificação:

```sh
cmake --build /home/diogo/Zith/build --target test-frontend-context -j 4
ctest --test-dir /home/diogo/Zith/build -R 'frontend-context' --output-on-failure
```

Resultado esperado: os testes frontend passam, incluindo imports de pacotes
usando as mesmas regras `import`, `from` e `export` dos módulos do workspace.

Verificações de falha:

- Se as raízes de pacotes ficarem visíveis num projeto sem entrada
  correspondente no lockfile, fazer a construção das raízes depender dos dados
  resolvidos do lockfile, não do conteúdo do armazenamento global.
- Se uma alteração de pacote ainda gerar um cache hit com dados de módulo
  obsoletos, acrescentar a identidade em falta a `CacheKey` antes de continuar.

Critério de sucesso: uma cópia limpa do projeto compila imports de pacotes com
código-fonte fixados sem exigir `--include` manual.

### Passo 5 - Definir Armazenamentos de Projeto e Global

Objetivo: o utilizador pode escolher instalação local ou global sem alterar a
versão resolvida por um projeto existente.

Subpassos:

1. Implementar a materialização do projeto no caminho escolhido no Passo 1.
2. Implementar o armazenamento associado ao compilador ou ao utilizador, de
   acordo com a política explícita de fallback para instalações do compilador
   só de leitura.
3. Organizar entradas do armazenamento global pela identidade imutável do
   pacote e pelos dados de compatibilidade do compilador exigidos pelo modelo de
   artefactos escolhido.
4. Tornar a instalação atómica, validar caminhos contra travessia de
   diretórios e rejeitar metadados corrompidos ou hashes de conteúdo
   divergentes.
5. Testar precedência local ao projeto, instalação global explícita,
   diretórios do compilador só de leitura, duas versões do compilador e
   resolução offline.

Resultado esperado: instalar a mesma dependência fixada em qualquer âmbito
produz a mesma identidade de pacote, e um pacote global não relacionado não
substitui uma entrada diferente do lockfile.

Verificações de falha:

- Se o diretório de instalação do compilador não permitir escrita, seguir a
  política de fallback escolhida e verificar o caminho final do armazenamento
  no teste.
- Se um projeto puder alterar inadvertidamente a materialização de outro,
  isolar o estado local do projeto do estado partilhado do armazenamento.

Critério de sucesso: os armazenamentos locais e globais afetam apenas a
reutilização de pacotes, não a seleção de dependências nem a semântica de
imports.

### Passo 6 - Acrescentar Artefactos Portáteis de Pacote

Objetivo: os publicadores podem distribuir opcionalmente um artefacto compilado
com metadados explícitos de compatibilidade, sem expor o formato do cache
interno do projeto.

Subpassos:

1. Especificar um contentor versionado para artefactos, metadados de
   integridade e campos de compatibilidade antes de escrever o serializador.
2. Manter a leitura e validação de artefactos no módulo de pacotes, não no
   armazenamento de cache do projeto.
3. Suportar primeiro código-fonte acompanhado de artefacto. Só acrescentar
   publicação de artefacto isolado depois de os erros de compatibilidade serem
   determinísticos e acionáveis.
4. Testar target e compilador compatíveis, incompatibilidade de target,
   incompatibilidade de compilador, conteúdo corrompido, fallback para
   código-fonte em falta e falha de artefacto isolado.

Resultado esperado: um artefacto compatível é carregado, um pacote híbrido
incompatível recorre ao código-fonte e um pacote só com artefacto indica qual
campo de compatibilidade falhou.

Verificações de falha:

- Se um artefacto contiver caminhos absolutos do projeto ou identidade do
  workspace, rejeitar o formato como não portátil e rever o contentor.
- Se não for possível derivar compatibilidade de forma reproduzível, adiar
  pacotes só com artefacto e manter o código-fonte obrigatório.

Critério de sucesso: a compatibilidade de artefactos é validada
independentemente dos hits do cache incremental normal.

### Passo 7 - Acrescentar Adaptadores de Build para Pacotes C

Objetivo: um pacote C declara headers e inputs de build sem tornar CMake
obrigatório para pacotes Zith ou para o resolver.

Subpassos:

1. Definir metadados de pacotes C separados da identidade independente da
   linguagem e da resolução de dependências.
2. Implementar um adaptador entre os metadados de pacotes C e os inputs FFI
   existentes, documentados em `/home/diogo/Zith/docs/18-c-interop.md`.
3. Reutilizar a importação de headers, `c_source_dirs`, defines, diretórios de
   bibliotecas e inputs de ligação atuais para a primeira forma de pacote C
   suportada.
4. Acrescentar um adaptador CMake apenas se o pacote C suportado exigir
   capacidades que os inputs FFI existentes não consigam expressar.
5. Se for acrescentado, invocar CMake como adaptador explícito com target e
   toolchain selecionados, e testar CMake em falta, falha de configuração,
   falha de build, headers gerados e target incompatível.
6. Alargar `/home/diogo/Zith/tests/test-c-compile.cpp` ou criar um teste C
   focado em pacotes para verificar visibilidade de headers, compilação de
   código-fonte C e ligação nativa.

Comandos exatos de verificação:

```sh
cmake --build /home/diogo/Zith/build --target test-c-compile test-packages -j 4
ctest --test-dir /home/diogo/Zith/build -R 'c-compile|package' --output-on-failure
```

Resultado esperado: o fixture de pacote C compila e liga através do adaptador
selecionado, sem alterar a forma de construir dependências só de Zith.

Verificações de falha:

- Se o pacote exigir scripts de build arbitrários, adiá-lo até existir uma
  política de segurança e reprodutibilidade para esses scripts.
- Se CMake não estiver disponível e o pacote declarar que exige o adaptador
  CMake, reportar esse pré-requisito sem afetar os restantes tipos de pacote.

Critério de sucesso: CMake continua opcional e substituível atrás do adaptador
de build C.

## Verificação Final de Aceitação

Antes de declarar o gestor de pacotes pronto para uma primeira versão,
verificar todos os pontos:

1. A biblioteca padrão não está declarada como dependência normal.
2. Um lockfile reproduz as identidades exatas dos pacotes selecionados.
3. Os armazenamentos locais e globais não alteram a resolução fixada no
   lockfile.
4. As raízes dos pacotes entram no resolver de imports existente sem alterar
   o ADR 0002.
5. O cache incremental do compilador continua separado dos artefactos de pacote
   distribuíveis.
6. Pacotes Zith com código-fonte funcionam sem CMake.
7. Pacotes C usam um adaptador explícito, e CMake só é necessário para pacotes
   que selecionem esse adaptador.
8. Passam os testes de pacotes, CLI, frontend e integração C.
