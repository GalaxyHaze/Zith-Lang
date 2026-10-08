## 16. Words (Custom Operators)

> **Status:** see [impl-status.md](impl-status.md) for the current status of words. Declarations
> are accepted but the body is dropped, and word call expressions are rejected.

Words let you define custom operators from identifiers. Each word has a fixed position, which is **prefix**, **infix**, or **suffix**, with language-defined precedence.

- You must activate a word with `use`, even if you already imported its module.
- Two words with the same name in the same scope: compile error.
- If the compiler sees any ambiguity (even potential), it errors out.
- Use a context for words when they participate in a domain-specific syntax
  integration. Contexts are not generic namespaces for public APIs. See
  [§17](17-contexts.md).

### 16.1 Word Types

| Type | Description | Example |
|---|---|---|
| `operator` | Overload a specific operator (`+`, `-`, `*`, `()`, etc.) | `implement Vec3 as Arithmetic { fn +(self, other: Self): Self { ... } }` |
| `token` | A word with low precedence that does nothing alone. Serves as a syntactic component in domain expressions. | `token SELECT;` |

#### Operator Words

Operator words overload built-in operators. Use `implement` with a capability to define the behavior:

```zith
implement Vec3 as Arithmetic {
    fn +(self, other: Self): Self { ... }
    fn -(self, other: Self): Self { ... }
    fn *(self, scalar: f32): Self { ... }
}

// Custom word — not overloading a built-in operator
use math.vec.dot as DOT;
use math.vec.cross as CROSS;

// Infix — reads as dot(vec1, vec2)
let d = vec1 DOT vec2;

// Prefix — reads as VALIDATE input
let result = VALIDATE data;

// Suffix — reads as input CHECK
let value = input CHECK;
```

| Position | Reads as |
|---|---|
| Infix | `a DOT b` → `dot(a, b)` |
| Prefix | `not x` → `not(x)` |
| Suffix | `x!` → `assert(x)` |

#### Token Words

Token words have low precedence and do nothing alone. They let domain syntax define terms such as SQL keywords:

```zith
token SELECT;
token FROM;
token WHERE;

// Operator* defines behavior for a token
operator* (SELECT, list) { ... }
```

> Tokens are useful when a domain syntax needs low-precedence keywords without function-call syntax.

### 16.2 Zith-- Macro Compatibility

Macros are available in the Zith-- subset, not in full Zith. This comparison
describes the subset only:

- **Zith-- macros:** Use call syntax and provide syntax-template expansion.
- **Words:** Work as keywords and can return values.

---

*[Zith Language Specification](Zith-spec.md) — Draft v0.9*
