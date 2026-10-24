# Revisão de Documentação e Exemplos — Zith--

Data: 2026-10-24
Escopo: docs/ (repo), Zith-website/, examples/

Este documento lista as inconsistências encontradas e o estado de cada uma.
As afirmações aqui foram verificadas contra o código (`src/`) e contra o
compilador (`./build/zithc --include stdlib check`), não apenas por leitura.

## 1. Inconsistências de conteúdo (corrigidas)

### 1.1 Website — `guide/raw-unsafe.md`
- `if (!value)` usava o prefixo `!`, que o Zith-- rejeita (`!` é reservado a
  postfix). Corrigido para `if (value is null)`.
- `return value;` devolvia `?i32` como `i32` (`E3001`). Corrigido para
  `return raw value;`.
- O snippet de `raw fn` tinha o corpo sem `raw`. Corrigido.
- `return raw value;` aparecia solto fora de função. Corrigido.

### 1.2 Website — `guide/control-flow.md`
- A sintaxe legada `(pattern) ~> body` era apresentada como a forma canónica.
  O `~>` emite `W1008`. Corrigido para `(pattern) body` com nota sobre a forma
  legada.
- `else if` era apresentado sem reserva; emite `W1008`. Corrigido.

### 1.3 Website — `guide/memory-model.md`
- A ordem do pipeline usava `NTA/NRA`; no Zith-- é `NTA/SRA`. Corrigido.

### 1.4 Website — `guide/types.md`
- `marker` é keyword reservada do Zith-- e não pode ser nome de binding.
  Renomeado para `flag`.
- Pack literals são posicionais; `|x: 10, y: 20, marker: 'x'|` é inválido.
  Corrigido para `|10, 20, 'x'|`.

### 1.5 Website — `zith--/reference.md`
- `let message: []char = response[1];` falhava: o literal de string é `*char`
  e não adapta fora de posição literal. Corrigido para `*char`.
- O tipo de pack nomeado usava `[]char` contra um literal `*char`. Corrigido.

### 1.6 Website — `guide/errors.md`
- O texto dizia que tipos failable `T!` são "working declared types"; `i32!` é
  parse error. Corrigido.
- `return port;` devolvia `?i32` como `i32` (`E3001`). Corrigido para
  `return raw port;`.

### 1.7 Website — `guide/c-interop.md`
- `printf("v=%d\n", 42);` descartava um resultado não-void (`E2026`).
  Corrigido para `_ = printf(...)`.

### 1.8 Website — `guide/syntax.md`
- Dizia que um escape desconhecido reporta `E0001`; o código real é `E0003`.
  Corrigido.

### 1.9 Website — `getting-started/introduction.md`
- "Ownership analysis (NRA)" usava NRA para o Zith--. Corrigido para SRA.

### 1.10 Website — `guide/overview.md`
- Frase começava com "the" minúsculo após ponto final. Corrigido.

### 1.11 Repo — `docs/03-type-system.md`
- `[_]T` (array de tamanho deduzido) não existe no Zith--; a linha da tabela
  foi removida.
- `string` é tipo full-Zith, não existe no Zith--; a secção foi reescrita para
  `*char`/`[]char` e as referências em exemplos foram corrigidas.

### 1.12 Repo — `docs/Zith-spec.md`
- A tabela de capítulos 02-21 não distinguia o spec full-Zith do subconjunto.
  Adicionada nota de escopo antes da tabela.

### 1.13 Repo — `docs/21-best-practices.md`
- O capítulo ensinava `%T`, `&T`, `&mut T`, `^T`, `try ... or`, `fail`, `catch`,
  `resume`, `use`, `risky()!` como se fossem correntes. O banner foi tornado
  explícito: o capítulo descreve full Zith e nenhuma dessas formas compila em
  Zith--.

### 1.14 Compilador — `src/diagnostics/error-codes.cpp`
- A mensagem de `E0004` citava as bases `(0x, 0o, 0b)`, mas o Zith-- usa `0c`
  para octal. Corrigido para `(0x, 0c, 0b)`.

## 2. Verificações que NÃO revelaram problema

Estas suspeitas da revisão inicial não se confirmaram; o conteúdo está correto:

- `guide/types.md` não declara `string` como builtin.
- `guide/control-flow.md` não usa `println` sem import.
- `guide/contexts-words-macros.md` já marca `tag macro` como rejeitado com
  `E2010` em Zith--.
- `guide/raw-unsafe.md` já separa `unsafe`/`Trust` como full-Zith.
- Os capítulos `docs/02` a `docs/20` já têm banners de status a apontar para
  `impl-status.md`.
- `guide/overview.md` lista corretamente a ordem de leitura (`guide-syntax`
  existe).
- `docs/Zith---implementation.md` já documenta `E4007 InvalidCallOwnership` para
  anotações de call; `E2010` é para outras rejeições.

## 3. Pendências reais (não corrigidas nesta iteração)

- **Exemplos em falta**: `const` fields, `extern var`/`extern const` com
  `= extern`, default parameters, slices variádicos, `@canonicalType`,
  `@sizeOf`/`@offsetOf`/`@alignOf`, `raw fn`, chamadas qualificadas por trait,
  `type`/`alias`, imports platform-specific, `#[discardable]`/`#[volatile]`,
  `match` como synonym, `for` flat, escape com `raw`, `is null` com `?*T`.
- **Exemplos negativos**: nenhum exemplo mostra o que não compila.
- **Organização**: não existe índice `docs/zith--/` no repo; `docs/Zith--.md`
  está em português e o website em inglês; falta página de comparação
  Zith-- vs full Zith.

## 4. Verificação executada

- `cmake --build build -j` — compila sem erros.
- `ctest --test-dir build` — 5 testes em falha, todos pré-existentes e
  reproduzidos com as alterações revertidas (`git stash`):
  `test-frontend-context`, `test-interface-satisfaction`,
  `test-hir-lower-modern`, `test-generic-hashmap`, `test-docs`. Nenhuma falha
  nova introduzida.
- `ctest --test-dir build -R test-examples` — passa a 100%.
- Cada snippet corrigido foi compilado individualmente com
  `./build/zithc --include stdlib check`.
