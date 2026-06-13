# Architecture Plan — Parser & Type System

## 1. Current State Assessment

### Pipeline (what exists)
```
PipelinePlan stages: Source → Lexed → Parsed → Resolved → TypeChecked → HirLowered → MirLowered → ZirInterpreted
```
But the actual stages in `CompilationSession::runTo()` call:
```
lexStage()  ✓ real
parseStage()  — which internally chains:
    scanStage()         ✓ real (registers symbols, creates UnbodyNodes)
    expandBodiesStage() ✓ real (re-parses fn bodies into AST)
    importStage()       ✓ real (resolves imports, merges symbols)
    solveStage()        ✗ stub (generics/macros/comptime — deferred)
semaStage()  ✗ stub
mirStage()   ✗ stub
zirStage()   ✗ stub
```

### Parser (what's actually implemented)
| Component | Status | Details |
|-----------|--------|---------|
| Token helpers (`peek`, `match`, `expect`, `consume`) | **done** | Full set |
| `scan()` — first-pass symbol registration | **done** | fn, struct, enum, union, component, trait, interface, import |
| `expandBodies()` — second-pass body parsing | **done** | Seeks to token offsets, re-parses fn bodies |
| `parsePrimary()` | **partial** | Blocks `{}`, literals, identifiers, parenthesized `(expr)` |
| `parseExpr(int min_prec)` | **stub** | Returns lhs from parsePrimary — **no infix/prefix parsing** |
| `parseStmt()` | **partial** | Handles `return`, `let`, expression-stmts |
| `parseFnDecl()` | **partial** | Name, params `()`, body `{}` — no return type, no generics |
| Struct/enum/union/trait decl parsing | **missing** | Only scanned (symbols registered), no full AST parse |
| Binary operators (infix) | **missing** | No Pratt infix loop |
| Unary prefix operators | **missing** | No `!`, `-`, `&`, `*` prefix parsing |
| Call expressions | **missing** | No `foo()` parsing |
| Field/index expressions | **missing** | No `a.b`, `a[i]` parsing |
| `if` expressions | **missing** | No if/else |
| `when` pattern matching | **missing** | Deferred |
| `flow fn` / marker / dock | **missing** | Deferred |
| `for` / `while` loops | **missing** | Deferred |

### Type System (what's actually implemented)
| Component | Status | Details |
|-----------|--------|---------|
| `TypeIntern` — type storage | **done** | Arena-backed `DynArray<TypeData>`, all type kinds |
| `TypeIntern` — convenience helpers | **done** | `internInt`, `internFloat`, `internPtr`, `internArray`, `internFn`, `internOptional`, `internFailable`, `internTypeVar` |
| `TypeIntern` — struct definitions | **done** | `defineStruct`, `addField`, `getField`, `hasField`, `fieldType` |
| `TypeIntern` — query | **done** | `lookup`, `kindOf` |
| `TypeIntern` — deduplication | **missing** | Every `intern()` call creates a new entry. By design, but needed for correctness. |
| `Unifier` — unify | **done** | Full Hindley-Milner style unification with occurs check |
| `Unifier` — substitute | **done** | Transitive substitution chain |
| `Unifier` — occurs check | **done** | Recursive through Ptr, Array, Fn, Optional, Failable |
| `Unifier` — isAssignable | **done** | With Never-as-bottom, bool-from-int8, struct by def_id |
| `Unifier` — isCoercible | **done** | Optional wrapping, int widening, float widening |
| `Unifier` — error reporting | **missing** | No diagnostic messages on unify failure |
| `TypeKind` enum + `TypeData` variant | **done** | 14 type kinds, full variant coverage |
| Predefined types | **done** | Error, Never, Void, Bool, Char at fixed low IDs |

---

## 2. Force Pipeline Isolation

### 2.1 Split `parseStage()` into real pipeline stages

The current `parseStage()` bundles 4 sub-stages. This violates pipeline isolation.

**Action:** Promote sub-stages into proper `Stage` enum entries.

In `src/cli/pipeline-plan.hpp`:
```
Stage::Source = 0
Stage::Lexed
Stage::Scanned          ← new (was hidden inside parseStage)
Stage::Expanded         ← new (was hidden inside parseStage)
Stage::Imported         ← new (was hidden inside parseStage)
Stage::Parsed           ← remaining: AST validation only
Stage::TypeChecked      ← rename from Resolved
Stage::HirLowered       ← rename from TypeChecked
Stage::MirLowered
Stage::ZirInterpreted
```

### 2.2 Stage contracts

| Stage | Input | Output | Pure? |
|-------|-------|--------|-------|
| `Lexed` | SourceFile | TokenStream | No (stateful lexer) |
| `Scanned` | TokenStream | Program (decl stubs + UnbodyNodes), SymbolTable | No (stateful scan) |
| `Expanded` | Program (UnbodyNodes) | Program (full AST with parsed bodies) | Yes (from fixed token offsets) |
| `Imported` | Program (ImportNodes) | SymbolTable (merged imports) | Yes (given visible roots) |
| `Parsed` | Program (full AST) | Validated AST | Yes |
| `TypeChecked` | AST + SymbolTable + TypeIntern | Type-annotated AST, SemaResult(HirModule) | Yes |
| `HirLowered` | Already done in TypeChecked | — | — |
| `MirLowered` | HirModule | MirModule | Yes |
| `ZirInterpreted` | MirModule | Executable/bytecode | Yes |

### 2.3 Rewrite `runTo()` in `compilation-session.cpp`

```cpp
bool CompilationSession::runTo(Stage target) {
    plan_.target = target;

    // Stage 1: Lex
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!lexStage()) return false;
    plan_.advance();

    // Stage 2: Scan (register symbols, create UnbodyNodes)
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!scanStage()) return false;        // ← was inside parseStage
    plan_.advance();

    // Stage 3: Expand bodies
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!expandBodiesStage()) return false; // ← was inside parseStage
    plan_.advance();

    // Stage 4: Import resolution
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!importStage()) return false;       // ← was inside parseStage
    plan_.advance();

    // Stage 5: Parse validation
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!parseStage()) return false;        // ← only validation now
    plan_.advance();

    // Stage 6: Type checking + HIR lowering
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!semaStage()) return false;         // ← wire real SemaPipeline
    plan_.advance();

    // Stage 7: MIR lowering
    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!mirStage()) return false;
    plan_.advance();

    // Stage 8: ZIR interpretation
    return zirStage();
}
```

### 2.4 Stage isolation rules (non-negotiable)

| Rule | Description |
|------|-------------|
| R1 | No stage reads tokens after `Expanded` stage finishes |
| R2 | No stage writes to `program_` after `Parsed` stage finishes |
| R3 | `semaStage` reads AST only — never calls parser functions |
| R4 | `mirStage` reads HIR only — never accesses AST or tokens |
| R5 | Each stage is independently testable: given input X, produce output Y |
| R6 | Stage output is immutable to subsequent stages (no back-patching) |
| R7 | If a stage fails, subsequent stages are skipped (already enforced) |

---

## 3. Complete the Parser

### 3.1 Add Pratt-style operator parsing

File: `src/parser/parser.cpp`

Replace the `parseExpr(int min_prec)` stub:

```cpp
namespace {

enum class Assoc : uint8_t { Left, Right };

struct InfixRule {
    ast::BinaryOp op;
    uint8_t prec;
    Assoc assoc;
};

uint8_t infixPrec(const lexer::Token &t) {
    if (!t.is(lexer::TokenKind::Punctuation)) return 0;
    switch (t.punc) {
    case '|':  return 4;
    case '^':  return 5;
    case '&':  return 6;
    case '<':  return 3;  // also <<, <=
    case '>':  return 3;  // also >>, >=
    case '+':  return 8;
    case '-':  return 8;
    case '*':  return 9;
    case '/':  return 9;
    case '%':  return 9;
    default:   return 0;
    }
}

ast::BinaryOp binaryOpForToken(const lexer::Token &t) {
    // Map token kind + punc to BinaryOp
    if (t.is(lexer::TokenKind::Punctuation)) {
        switch (t.punc) {
        case '+': return ast::BinaryOp::Add;
        case '-': return ast::BinaryOp::Sub;
        case '*': return ast::BinaryOp::Mul;
        case '/': return ast::BinaryOp::Div;
        case '%': return ast::BinaryOp::Rest;
        // & | ^ << >>
        }
    }
    if (t.is(lexer::TokenKind::Operator)) {
        // == != < <= > >= && ||
    }
}
```

```cpp
ast::ExprId Parser::parseExpr(int min_prec) {
    auto lhs = parsePrefix();

    while (true) {
        if (eof()) break;

        // Postfix call: foo()
        if (peek().punc == '(') {
            advance();
            memory::DynArray<ast::ExprId> args{bld->arena()};
            if (peek().punc != ')') {
                args.push(parseExpr());
                while (consume(','))
                    args.push(parseExpr());
            }
            consume(')');
            lhs = bld->call(lhs, std::move(args));
            continue;
        }

        // Postfix field: a.b
        if (peek().punc == '.') {
            advance();
            auto field = lexeme();
            advance();
            lhs = bld->field(lhs, field);
            continue;
        }

        // Postfix index: a[i]
        if (peek().punc == '[') {
            advance();
            auto index = parseExpr();
            consume(']');
            lhs = bld->index(lhs, index);
            continue;
        }

        // Postfix unwrap: expr!
        if (peek().punc == '!') {
            advance();
            // wrap in unary "unwrap" — or leave as postfix op
            continue;
        }

        // Infix binary operator
        uint8_t prec = infixPrec(peek());
        if (prec == 0 || prec < min_prec) break;

        auto op = binaryOpForToken(peek());
        advance();
        auto rhs = parseExpr(prec + 1);
        lhs = bld->binary(lhs, op, rhs);
    }

    return lhs;
}

ast::ExprId Parser::parsePrefix() {
    // Unary prefix: !expr, -expr, &expr, *expr
    if (peek().punc == '!') { advance(); return bld->unary(ast::UnaryOp::Not, parseExpr(10)); }
    if (peek().punc == '-') { advance(); return bld->unary(ast::UnaryOp::Neg, parseExpr(10)); }
    if (peek().punc == '&') { advance(); return bld->unary(ast::UnaryOp::Ref, parseExpr(10)); }
    if (peek().punc == '*') { advance(); return bld->unary(ast::UnaryOp::Deref, parseExpr(10)); }

    return parsePrimary();
}
```

### 3.2 Add `if` expressions

```cpp
ast::ExprId Parser::parseIf() {
    advance();  // consume 'if'

    auto cond = parseExpr();
    consume('{');  // if bodies must be braced
    auto then_branch = parseBlock();

    ast::ExprId else_branch = kInvalidExpr;
    if (peek().is(TokenKind::Control) && lexeme() == "else") {
        advance();
        if (peek().is(TokenKind::Control) && lexeme() == "if")
            else_branch = parseIf();  // else if
        else
            else_branch = parseBlock();
    }

    return bld->ifExpr(cond, then_branch, else_branch);
}
```

Wire it into `parseStmt()`:
```cpp
if (peek().is(TokenKind::Control) && lexeme() == "if")
    return bld->addStmt(parseIf());
```

### 3.3 Add struct/enum/union/trait/impl declaration parsing

These declarations are already **scanned** (symbols registered + UnbodyNodes created).
Add full parse functions that use the same UnbodyNode mechanism:

```cpp
ast::DeclId Parser::parseStructDecl() {
    // consume 'struct', name
    // if '{' → parse field list with types
    auto fields = parseStructFields();  // ident: Type, ...
    return bld->structDecl(name, std::move(fields));
}
```

The `expandBodies()` function must be extended to call these for struct/enum/union/trait entries in `ScanResult`.

### 3.4 Add `for` / `while` loops

```cpp
ast::ExprId Parser::parseFor() {
    advance();  // 'for'
    consume('(');
    auto init = parseStmt();
    auto cond = parseExpr();
    consume(';');
    auto step = parseExpr();
    consume(')');
    auto body = parseBlock();
    // Desugar into block with let + while
}

ast::ExprId Parser::parseWhile() {
    advance();  // 'while'
    auto cond = parseExpr();
    auto body = parseBlock();
    // → bld->whileExpr(cond, body)
}
```

### 3.5 Add error recovery

```cpp
if (!consume(';'))
    recovery::panic(*tok, {TokenKind::End, TokenKind::Punctuation(';')});
```

Sync sets:
- Top-level: `fn`, `struct`, `enum`, `import`, `pub`, `mod`
- Statement: `;`, `}`, `let`, `return`, `if`, `for`, `while`
- Expression: `)`, `]`, `}`, `;`, `,`

---

## 4. Complete the Type System

### 4.1 Add deduplication to `TypeIntern`

In `src/types/type-intern.cpp`:

```cpp
TypeId TypeIntern::intern(TypeData data) {
    // Linear scan for structural equality
    for (TypeId i = 0; i < types_.size(); i++) {
        if (types_[i] == data)  // requires adding operator== to TypeData
            return i;
    }
    TypeId id = static_cast<TypeId>(types_.size());
    types_.push(std::move(data));
    return id;
}
```

Requires adding structural `operator==` to all variant members or generating it from the variant.

**Impact:** Existing test `test_type_intern_int()` asserts no dedup. Must update this test.

### 4.2 Add error messages to `Unifier`

In `src/types/unify.cpp`:

```cpp
bool Unifier::unify(TypeId a, TypeId b, memory::Span span) {
    a = substitute(a);
    b = substitute(b);

    if (a == b) return true;

    auto kind_a = intern_.kindOf(a);
    auto kind_b = intern_.kindOf(b);

    if (kind_a == TypeKind::TypeVar) {
        if (occurs(a, b)) {
            diags_.report(Severity::Error, CyclicType,
                "cyclic type: cannot unify type variable with type that contains it",
                span);
            return false;
        }
        // bind
        return true;
    }

    if (kind_a != kind_b) {
        diags_.report(Severity::Error, TypeMismatch,
            "type mismatch: expected " + kindName(kind_a) + " but got " + kindName(kind_b),
            span);
        return false;
    }

    // ... rest of existing unify logic
}
```

### 4.3 Add helper to `TypeIntern`

```cpp
std::string_view TypeIntern::kindName(TypeKind k, TypeId id) const {
    switch (k) {
    case TypeKind::Int: {
        auto &t = std::get<TypeInt>(lookup(id));
        switch (t.width) {
        case IntWidth::I8:  return "i8";   case IntWidth::U8:  return "u8";
        case IntWidth::I16: return "i16";  case IntWidth::U16: return "u16";
        case IntWidth::I32: return "i32";  case IntWidth::U32: return "u32";
        case IntWidth::I64: return "i64";  case IntWidth::U64: return "u64";
        case IntWidth::I128: return "i128"; case IntWidth::U128: return "u128";
        }
    }
    case TypeKind::Struct: return getStructDef(id).name;
    // ... etc
    }
}
```

---

## 5. Wire SemaPipeline to Real AST Traversal

### 5.1 Replace stub in `src/sema/sema-pipeline.cpp`

```cpp
SemaResult SemaPipeline::run(ast::DeclId program_root) {
    auto &program = ctx_.builder().getDecl(program_root);

    for (auto decl_id : ctx_.builder().programDecls()) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        std::visit([this](auto &d) { visitDecl(d); }, decl);
    }

    return SemaResult{std::move(hir_), std::move(ctx_.diags()), std::move(ctx_.syms())};
}
```

### 5.2 AST → HIR visitor

```cpp
HirExprId SemaPipeline::visitExpr(ast::ExprId id) {
    auto &node = ctx_.builder().getExpr(id);
    return std::visit([this](auto &n) -> HirExprId {
        using T = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<T, ast::LitValue>) {
            HirLiteral lit{};
            switch (n.kind) {
            case ast::LitKind::Int:
                lit.type = ctx_.types().internInt(IntWidth::I64);
                lit.i = std::strtoll(std::string(n.raw).c_str(), nullptr, 10);
                break;
            case ast::LitKind::Float:
                lit.type = ctx_.types().internFloat(FloatWidth::F64);
                lit.f = std::strtod(std::string(n.raw).c_str(), nullptr);
                break;
            case ast::LitKind::Bool:
                lit.type = kBoolType;
                lit.b = (n.raw == "true");
                break;
            case ast::LitKind::String:
                // intern string constant
                break;
            case ast::LitKind::Char:
            case ast::LitKind::Nil:
                break;
            }
            return hir_.addExpr(HirLiteral{lit});
        }

        if constexpr (std::is_same_v<T, ast::BinaryNode>) {
            auto lhs = visitExpr(n.lhs);
            auto rhs = visitExpr(n.rhs);
            // type-check lhs and rhs
            HirBinaryOp op = mapBinaryOp(n.op);
            hir_.addExpr(HirBinary{lhs, rhs, op});
        }

        // ... IfNode, CallNode, IdentNode, etc.

        return kInvalidHirExpr;
    }, node);
}
```

### 5.3 Type-checking during lowering

```cpp
void SemaPipeline::checkBinopTypes(HirExprId lhs, HirExprId rhs, HirBinaryOp op) {
    auto lhs_type = getHirType(lhs);
    auto rhs_type = getHirType(rhs);
    auto result_type = ctx_.types().internInt(IntWidth::I64);  // default

    if (!unifier_.unify(lhs_type, rhs_type, getSpan(lhs))) {
        // error already reported by Unifier
        return;
    }

    // Set result type
    setHirType(lhs, result_type);
}
```

---

## 6. Testing Strategy

### 6.1 New test files

| File | What it tests |
|------|---------------|
| `tests/unit/parser-binary.cpp` | Binary expressions at all precedence levels |
| `tests/unit/parser-calls.cpp` | Function calls, field access, indexing |
| `tests/unit/parser-if.cpp` | if/else if/else |
| `tests/unit/parser-decls.cpp` | struct, enum, union, trait declarations |
| `tests/unit/type-dedup.cpp` | TypeIntern deduplication |
| `tests/unit/unify-errors.cpp` | Unifier error messages |
| `tests/unit/sema-hir.cpp` | Full AST→HIR lowering with type checking |

### 6.2 Testing pattern

```cpp
static void test_parse_binary_add() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto tokens = lexString("1 + 2");
    Parser p(&tokens, &bld, &diags);
    auto expr = p.parseExpr();

    CHECK(expr != kInvalidExpr, "parsed binary expression");
    CHECK(bld.getExpr(expr).index() == /* BinaryNode */, "is binary node");

    auto &bin = std::get<ast::BinaryNode>(bld.getExpr(expr));
    CHECK_EQ(bin.op, ast::BinaryOp::Add, "operator is +");
    CHECK(std::get<ast::LitValue>(bld.getExpr(bin.lhs)).kind == ast::LitKind::Int, "lhs is int lit");
}
```

### 6.3 Stage-boundary test pattern

```cpp
static void test_pipeline_scan_expand_isolation() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto tokens = lexString("fn foo() { return 1; }");
    Parser p(&tokens, &bld, &diags);
    auto scan_result = scan(p, syms);

    // After scan: fns have UnbodyNodes, no real bodies
    CHECK_EQ(scan_result.fns.size(), 1u, "one fn scanned");
    auto &entry = scan_result.fns[0];
    CHECK(std::get_if<ast::UnbodyNode>(&bld.getExpr(entry.body_node)) != nullptr,
          "body is UnbodyNode");

    // After expand: UnbodyNode replaced with real BlockNode
    p.expandBodies(scan_result);
    auto &body = bld.getExpr(entry.body_node);
    CHECK(std::get_if<ast::BlockNode>(&body) != nullptr,
          "body is now BlockNode");
}
```

---

## 7. Implementation Order

```
Step 1: Split pipeline stages
  File: src/cli/pipeline-plan.hpp (update Stage enum)
  File: src/cli/compilation-session.cpp (rewrite runTo, promote sub-stages)

Step 2: Complete Pratt expression parsing
  File: src/parser/parser.cpp (parseExpr, parsePrefix, infix handling)
  File: src/parser/parser.hpp (no changes needed)
  Test: tests/unit/parser-expr.cpp (add binary/call/field/index tests)

Step 3: Add if expressions
  File: src/parser/parser.cpp (parseIf, wire into parseStmt)
  Test: tests/unit/parser-expr.cpp (add if-test)

Step 4: Complete declaration parsing
  File: src/parser/parser.cpp (parseStructDecl, etc.)
  File: src/parser/parser.hpp (no changes needed)
  File: src/parser/parser.cpp (extend expandBodies for struct/enum/union/trait)
  Test: tests/unit/parser-stmt.cpp (add decl tests)

Step 5: Add loops + error recovery
  File: src/parser/parser.cpp (parseFor, parseWhile, recovery calls)
  Test: tests/unit/parser-stmt.cpp (add loop tests)

Step 6: Add TypeIntern deduplication
  File: src/types/type-intern.cpp (dedup in intern())
  File: tests/unit/sema-basics.cpp (update test_intern_int, add dedup tests)

Step 7: Add Unifier error reporting
  File: src/types/unify.cpp (thread span, add diagnostic reports)
  File: src/types/unify.hpp (add span param to unify)
  Test: tests/unit/sema-basics.cpp (add error tests)

Step 8: Wire SemaPipeline
  File: src/sema/sema-pipeline.cpp (full visitor implementation)
  File: src/sema/sema-pipeline.hpp (no changes needed)
  File: src/cli/compilation-session.cpp (wire semaStage)
  Test: tests/unit/sema-hir.cpp (new file)
```

---

## 8. Key Design Invariants

| Invariant | Description |
|-----------|-------------|
| I1 | `scan()` is the **only** function that registers symbols in `SymbolTable` |
| I2 | `expandBodies()` is the **only** function that replaces `UnbodyNode` with real AST |
| I3 | The parser **never** calls into `TypeIntern` or `Unifier` — that's sema's job |
| I4 | `TypeIntern` is the **only** source of `TypeId` values |
| I5 | `Unifier` is the **only** source of type equation solving |
| I6 | `SemaPipeline` is the **only** bridge between AST and HIR |
| I7 | HIR → MIR lowering is a **pure function** (no access to AST, tokens, or source) |
| I8 | Each pipeline stage reads only its predecessor's output — no skipping or re-entering |
