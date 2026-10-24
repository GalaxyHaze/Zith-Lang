# Documento de Inconsistencias — Docs/Specs/Exemplos

Data: 2026-10-24
Escopo: docs/Zith--.md, docs/Zith---implementation.md, docs/impl-status.md, docs/nra-spec.md, docs/07-memory-model.md, exemplos e testes relacionados.

## Resolvido

### 1) Terminologia NRA vs SRA (Alta prioridade)
- docs/Zith--.md foi atualizado: secções Ownership e "Não É Desta Iteração" usam SRA.
- Foi adicionada nota: SRA é o subconjunto do Zith--; NRA é o full Zith.
- Ajustado em: docs/Zith--.md.

### 2) dyn dispatch — Spec only vs Working (Alta prioridade)
- docs/impl-status.md: removida linha duplicada/outdated de `dyn dispatch` da tabela Spec Only.
- Mantido como `dyn Trait`/`dyn Interface` Working na tabela Language Features.

### 3) const fn — fora vs parseado (Media prioridade)
- docs/impl-status.md: `const fn` alterado de "Parse-level in progress" para "Parse error".
- Mantido alinhado com docs/Zith--.md e implementation notes.

### 4) component nao documentado (Media prioridade)
- docs/Zith--.md: adicionada secção `### Component` com exemplo e restrições.

### 5) type e alias nao documentados (Media prioridade)
- docs/Zith--.md: adicionada secção `### Type wrapper e alias` com exemplo.

### 6) raw fn nao documentado (Baixa prioridade)
- docs/Zith--.md: adicionada secção `### raw fn` com exemplo.

### 7) while deprecated nao documentado (Baixa prioridade)
- docs/Zith--.md: adicionada nota sobre `while` com `W1008`.

### 8) Intrinsecos omitidos em docs/Zith--.md (Media prioridade)
- docs/Zith--.md: adicionados `@sizeOf`, `@offsetOf`, `@alignOf` e `@canonicalType`.

### 9) Forma flat de `for` nao documentada (Baixa prioridade)
- docs/Zith--.md: mencionada forma flat `for (init, cond, step)`.

### 11) Warnings de atributos nao documentadas em docs/Zith--.md (Baixa prioridade)
- docs/Zith--.md: adicionada secção `Diagnósticos e warnings` com `W2028`/`W2029`.

### 12) Diagnosticos omitidos em docs/Zith--.md (Baixa prioridade)
- docs/Zith--.md: adicionada lista de diagnósticos relevantes.

### 14) match como parser synonym nao mencionado (Baixa prioridade)
- docs/Zith--.md: adicionada nota sobre `match` como synonym de `when`.

### 15) dock como expression nao mencionado (Baixa prioridade)
- docs/Zith--.md: adicionada nota sobre `dock State(args)` como expressão.

### 16) jump terminating transitions nao mencionado (Baixa prioridade)
- docs/Zith--.md: adicionada nota sobre `jump Next(args)` e validação de aridade/tipos.
