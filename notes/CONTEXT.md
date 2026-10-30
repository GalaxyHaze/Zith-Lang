# CONTEXT — glossário do site

Vocabulário partilhado do redesenho do site do Zith. Manter curto e sem
ambiguidade. Documentos de trabalho vivem em `notes/`; `docs/` é a área de
conteúdo do site.

## Produto

**Zith**:
Linguagem de programação compilada, tipada estaticamente, para sistemas.

**NRA (Node Resource Analysis)**:
Análise de compilação que prova segurança de memória e tempos de vida.
_Avoid_: borrow checker.

**zithc**:
O compilador de Zith.

**zith-lsp**:
Servidor de linguagem (Language Server Protocol) para Zith.

**Glarea**:
Nome novo do IDE no browser. Substitui o nome antigo **Helios** em toda a
interface. _Avoid_: Helios (nome descontinuado).

**Playground**:
Corre Zith no browser via WebAssembly.

## Site

**Portal**:
A landing page (`index.html`) — entrada do site.
_Avoid_: home, homepage.

**Docs**:
Área de documentação com uma entrada neutra e áreas próprias para Zith e
Zith--, entre as quais se pode alternar.

**Docs neutros**:
Área própria de documentação que apresenta Zith e Zith--, explica a relação
entre ambas e reúne tópicos gerais como FAQ, instalação, CLI e estado de
implementação.

**Documentação Zith**:
Explica a linguagem completa, incluindo o seu desenho, especificação e
perguntas específicas dessa linguagem. Funcionalidades ainda não suportadas
por `zithc` são descritas como parte do desenho, não como capacidades atuais.

**Documentação Zith--**:
Explica o subconjunto de Zith suportado pelo compilador `zithc`, incluindo
o primeiro programa, guias práticos e perguntas específicas desse subconjunto.

**Dev Log**:
O blog de desenvolvimento (`about/`). _Avoid_: blog, diário.

**Changelog**:
Notas de versão do `zithc` e da linguagem.

**Desktop OS Mode**:
Antiga página retro com ícones, janelas arrastáveis e efeitos CRT
(`html/home.html`). **Removida** neste redesenho. _Avoid_: Retro OS.

## Tema

**Tokens de design**:
Variáveis CSS (`--*`) que definem cor e tipografia. Fonte única de verdade.

**Cromo**:
O header e o footer partilhados. _Avoid_: chrome, barra de navegação.

**Arquivo**:
Material retirado do design mas preservado em `notes/archive/`, fora do site.
