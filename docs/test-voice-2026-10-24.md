# Teste de voz — docs Zith--

Data: 2026-10-24
Objetivo: verificar se o tom, a estrutura e o nível de detalhe estão alinhados com a documentação existente do Zith--.

## Bindings

`let`, `var` e `const` são as formas de binding disponíveis no Zith--.

- `let` cria um binding local imutável e não-rebindável.
- `var` cria um binding local mutável e rebindável.
- `const` cria um binding imutável com storage estático, que pode ser global ou local, com inicializador obrigatório.

```zith
fn main(): i32 {
    let a = 1;
    var b = 2;
    const LOCAL = 4;
    b = 3;
    a + b + LOCAL
}
```

Um binding `let` sem inicializador aceita a primeira escrita para inferir o
tipo; escritas seguintes são rejeitadas. A imutabilidade propaga-se a caminhos
de escrita em valores compostos: `let p; p.x = 1` é rejeitado quando `p` é
`let` ou `const`, mas continua permitido para `var`.

## Ownership em chamadas

`lend` e `view` são os qualificadores de ownership disponíveis no Zith--. Um
parâmetro `p: lend T` ou `p: view T` recebe o valor por referência: `lend`
permite escrita no referente e `view` não.

Um binding `default` é um binding com modo de ownership implícito. Quando esse
binding é passado a um parâmetro `lend`/`view`, o call site exige a anotação
correspondente:

```zith
struct Sample {
    x: i32
}

fn bump(p: lend Sample): i32 { p.x = p.x + 1; p.x }
fn read(p: view Sample): i32 { p.x }

fn main(): i32 {
    var q = Sample { x: 41 };
    _ = bump(lend q);
    _ = read(view q);
    q.x
}
```

Literais, temporários e resultados de chamada não exigem anotação. A anotação
errada reporta `E4005 OwnershipCoercionRequired`; `unique`, `share`, `belong` e
`mut` em argumento de call são rejeitados com `E4007 InvalidCallOwnership`.
