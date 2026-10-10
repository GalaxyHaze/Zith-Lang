# ADR 0002 — Paleta única, clara e flat

Data: 2026-10-10
Estado: aceite

## Contexto

O site antigo tem 7 blocos `:root` em ficheiros CSS diferentes, ~9 famílias de
matiz sem papéis atribuídos e vários pares reprovam contraste WCAG. O protótipo
tem uma direção mais quente (areia + teal) mas os seus tokens vivos divergem das
próprias notas de design.

## Decisão

Uma só paleta, clara e flat, num único `:root` em `css/tokens.css`:

- Fundo azul-gelo (`--bg #eef4f7`), superfícies brancas (`--surface #ffffff`).
- Texto azul-grafite (`--text #16232e`), secundário `--text-muted #4b6070`,
  títulos `--title #1b2b4d`.
- Cromo em indigo (`--header-bg #26324f`, `--footer-bg #1a2130`).
- Marca laranja (`--brand #e2662e`) **só como acento/superfície**, nunca texto
  normal sobre claro; acento funcional teal (`--accent #0f6e63`).

Todos os pares de texto validam AA ou AAA (o par mais fraco é `--brand` sobre
`--surface` a 3.40:1, aceitável para UI/texto grande).

## Consequências

- Trocar de tema passa a ser editar valores num só ficheiro.
- A auditoria de contraste fica centralizada e verificável.
- A identidade fica mais fria/limpa que o protótipo; o laranja sobrevive como
  assinatura.

## Alternativas rejeitadas

- **Copiar a paleta quente do protótipo**: menos "profissional" e com notas
  divergentes do código.
- **Manter tokens por página**: perpetua o drift que motivou a mudança.
