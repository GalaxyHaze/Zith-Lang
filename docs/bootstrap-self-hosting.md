# Bootstrap E O Núcleo Do Compilador

Esta nota regista uma hipótese arquitetural a revisitar se Zith avançar para
uma implementação do compilador escrita na própria linguagem. Não é uma
decisão arquitetural aceite nem um plano de implementação.

## Pressuposto

Aqui, *bootstrap* significa implementar o compilador em Zith e compilar essa
implementação com um compilador Zith existente. Se bootstrap significar apenas
iniciar o runtime ou a standard library, esta nota não identifica a dívida
relevante.

## Forma Atual

`CompilationSession` orquestra o pipeline do compilador. A sua interface e o
seu estado também ligam a compilação a opções, caminhos do projeto,
diagnósticos, arenas, símbolos, tipos, HIR, análise semântica, cache, saída,
linking e execução. Consulta
[`compilation-session.hpp`](../src/session/compilation-session.hpp) e a
descrição do pipeline em [`AGENTS.md`](../AGENTS.md).

`FrontendContext` já é um módulo distinto para o frontend moderno. As divisões
mecânicas dos ficheiros de `CompilationSession` e de outras unidades de
tradução grandes também estão concluídas. Consulta
[`monolith-splits.md`](plans/monolith-splits.md) e a secção sobre dívida
estrutural em [`implementation-debt.md`](implementation-debt.md).

Por isso, a preocupação não é o tamanho do ficheiro da sessão. É a
possibilidade de a capacidade de compilação ser mais difícil de reutilizar e
testar de forma independente do que a divisão do pipeline em etapas sugere.

## Hipótese A Reavaliar

Antes de ampliar o trabalho de self-hosting, considerar disponibilizar o
núcleo do compilador através de uma interface pequena e estável. Um chamador
deve poder fornecer os dados de entrada da compilação e receber diagnósticos
e um resultado, sem também ter de gerir o comportamento da linha de comandos,
a execução de processos ou o linking nativo.

`CompilationSession` pode continuar como módulo externo de orquestração. A
seam entre a sessão e o núcleo deve ocultar os detalhes das etapas sem
impedir testes internos ao nível das etapas. Não criar uma interface pública
para cada etapa nem repetir as divisões de unidades de tradução já concluídas.

Esta seam pretende melhorar a alavancagem e a localidade: o driver atual, os
testes e um futuro driver de bootstrap podem exercitar o mesmo comportamento
de compilação através de uma única interface. Não é um pré-requisito para a
primeira experiência de self-hosting, que pode invocar o executável atual do
compilador.

## Validação Antes De Adotar O Desenho

Quando existir um alvo concreto de bootstrap:

- Inventariar as funcionalidades de Zith, os recursos de runtime e as
  interfaces com funções externas de que o alvo precisa. Uma lacuna na
  linguagem pode ser um bloqueio mais imediato do que a arquitetura do
  compilador.
- Descrever os comandos de build das etapas 1 e 2 e os artefactos que cada
  etapa consome e produz.
- Verificar se o driver de bootstrap precisa de uma interface de compilação
  dentro do processo, ou se pode usar o executável atual sem acoplamento
  excessivo.
- Prototipar a interface mais estreita que suporte o driver atual e o caminho
  de bootstrap. Manter fora dela o sistema de ficheiros, o linker e a gestão
  de processos, salvo quando um caso de uso concreto o exigir.
- Testar diagnósticos e resultados de compilação através dessa interface.
  Depois, acrescentar um teste ponta a ponta que compile a implementação do
  compilador com o compilador da etapa anterior.

Transformar esta hipótese num plano de implementação ou ADR apenas se essa
validação identificar um problema real de reutilização ou de testabilidade.

## Fora De Âmbito

- Repetir as divisões de monólitos já concluídas.
- Reescrever o pipeline do compilador ou alterar a semântica da linguagem
  como parte desta mudança arquitetural.
- Considerar o self-hosting provado apenas porque a etapa 1 compila um
  programa.
- Escolher nomes finais de tipos ou fixar uma interface pública antes de
  conhecer os casos de uso do bootstrap.
