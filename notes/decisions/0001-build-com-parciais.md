# ADR 0001 — Build com parciais para o header e o footer

Data: 2026-10-10
Estado: aceite

## Contexto

O site tem o header e o footer copiados à mão por ~154 ficheiros HTML, o que
gerou deriva entre páginas. Queremos uma fonte única para o cromo, HTML puro no
resultado, e pré-visualização local simples. No GitHub Pages não se consegue ao
mesmo tempo markup partilhado + estático puro + zero build.

## Decisão

Adoptar o sistema de parciais do protótipo: `partials/header.html` e
`partials/footer.html` expandidos por `tools/build.py` (Python, sem
dependências) para `_site/`. O deploy passa a ser feito por GitHub Actions, que
corre o build e publica `_site/` (Pages deixa de servir a branch diretamente).

## Consequências

- Header e footer passam a ter uma só fonte; a página ativa é um parâmetro do
  include.
- O resultado publicado continua a ser HTML puro e pré-visualizável com
  `python3 -m http.server`.
- O Pages tem de mudar de "Deploy from a branch" para "GitHub Actions".
- As páginas-fonte mantêm os URLs atuais; o build não altera caminhos.

## Alternativas rejeitadas

- **Jekyll `_includes/`**: exigia Ruby/Jekyll e Liquid a processar todo o HTML.
- **Injeção por JS (`fetch` + `innerHTML`)**: FOUC/CLS e header invisível sem JS.
- **Duplicar à mão**: perpetua a deriva que motivou esta decisão.
