# ADR 0004 — Reintroduzir o efeito de máquina de escrever, com âmbito restrito

Data: 2026-10-10
Estado: aceite

## Contexto

O ADR 0003 removeu o `js/typewriter.js` por ser um efeito retro. O pedido atual
é recuperar a "aparição caractere a caractere", mas apenas em dois sítios:
o código do **Code Spotlight** e os **comandos de instalação**.

## Decisão

Recriar um motor mínimo, `js/typewriter.js`, com uma única função
`ZithTypewriter.type(element, text, options)`. O chamador decide como cada
prefixo é renderizado (`options.render`); por omissão escreve `textContent`.

- O Code Spotlight passa um `render` que reutiliza `js/zith-highlight.js`, para
  o código ficar colorido enquanto cresce (em vez de re-parsear HTML como a
  versão antiga).
- Os comandos de instalação animam ao trocar de separador; o comando inicial
  vem do HTML, sem flash.

O caret é CSS puro (`.is-typing::after` em `css/base.css`), com `currentColor`
para ser legível tanto no bloco escuro do spotlight como na superfície clara do
comando. Respeita `prefers-reduced-motion` (revela de imediato).

## Consequências

- O efeito existe só onde foi pedido; nenhum outro elemento o usa.
- Um ficheiro JS pequeno (≈40 linhas) em vez das ~160 da versão antiga.

## Alternativas rejeitadas

- **Restaurar o `js/typewriter.js` antigo**: re-parseava HTML com `setTimeout`
  por token e trazia máquina de estados não usada.
- **Animação global em `.typewriter`**: reintroduziria o efeito em toda a página,
  contra o objetivo profissional/flat.
