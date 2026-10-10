# ADR 0003 — Retirar o modo desktop e os efeitos retro

Data: 2026-10-10
Estado: aceite

## Contexto

O site tem um "Desktop OS Mode" (`html/home.html`) com ecrã de boot falso,
janelas arrastáveis, ícones de secretária e efeitos CRT (scanlines, glow, ruído,
vignette). Tem ainda áudio ambiente e um efeito de máquina de escrever. O
objetivo do redesenho é um site profissional e flat.

## Decisão

Remover o modo desktop e a maquinaria retro: `html/home.html`, `css/home.css`,
`js/loader.js`, `js/windows.js`, `js/typewriter.js`, `js/goHome.js`, o
`crt-overlay` e o painel de efeitos (`js/settings.js`). Os ícones em emoji
passam a SVG.

**Exceção:** o botão de música ambiente é mantido (é apreciado); `js/ambient.js`
e `media/audio/ambient.mp3` ficam, reestilizados de forma flat e discreta.

## Consequências

- O site perde uma página (`html/home.html`) e ~5 ficheiros JS/CSS retro.
- O `changelog.html` deixa de ser injetado no modo desktop; passa a página
  própria.
- O áudio ambiente fica desligado por omissão e só toca após interação.

## Alternativas rejeitadas

- **Manter o modo desktop**: incompatível com o objetivo "profissional e flat".
