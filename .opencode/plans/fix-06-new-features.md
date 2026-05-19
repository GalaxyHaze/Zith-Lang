# Fix Plan for 06_new_features.zith (23 errors)

## Overview
The test file `examples/06_new_features.zith` produces 23 errors when compiled. This document outlines the root causes and specific code fixes needed.

---

## Fix 1: Pratt Parser - Handle Statement Terminators

**File**: `impl/parser/parser_expr.cpp`  
**Line**: 368  
**Severity**: HIGH - Causes 6+ cascading errors

### Problem
The `parse_expr_bp` function unconditionally calls `parse_nud(p)`, which errors when the current token is a statement terminator (`;`, `}`, or EOF).

### Current Code (line 368-369):
```cpp
static ZithNode *parse_expr_bp(Parser *p, const int min_bp) {
    ZithNode *left = parse_nud(p);
```

### Fix:
Add a check for statement terminators before calling `parse_nud`:

```cpp
static ZithNode *parse_expr_bp(Parser *p, const int min_bp) {
    // Check for statement terminators - don't try to parse them as expressions
    if (parser_check(p, ZITH_TOKEN_SEMICOLON) || parser_check(p, ZITH_TOKEN_RBRACE) ||
        parser_check(p, ZITH_TOKEN_END)) {
        return nullptr;
    }
    ZithNode *left = parse_nud(p);
```

### Impact
This will fix errors like:
- `06_new_features.zith:39:11: error: unexpected token ';'`
- `06_new_features.zith:45:5: error: expected ';' (got 'return')`

---

## Fix 2: Struct Literal Parsing - Check for `{` After Identifier

**File**: `impl/parser/parser_expr.cpp`  
**Line**: 261-287  
**Severity**: HIGH - Causes 9 struct literal errors

### Problem
The struct literal parsing code at lines 276-286 looks correct, but may not be reached due to Fix 1's cascading effects. After Fix 1 is applied, verify that struct literals parse correctly.

### Current Code (line 261-287):
```cpp
case ZITH_TOKEN_IDENTIFIER: {
    ZithNode *ident = zith_ast_make_identifier(p->arena, loc, t->lexeme.data, t->lexeme.len);
    if (parser_match(p, ZITH_TOKEN_LPAREN)) {
        // ... function call handling ...
    }
    if (parser_match(p, ZITH_TOKEN_LBRACE)) {
        // Struct literal: TypeName{ ... }
        size_t field_count = 0;
        ZithNode **field_inits = parse_struct_lit_fields(p, &field_count);
        parser_expect(p, ZITH_TOKEN_RBRACE, "expected '}' closing struct literal");
        ZithStructLitPayload slp = {};
        slp.type_spec = ident;
        slp.field_inits = field_inits;
        slp.field_count = field_count;
        return zith_ast_make_struct_lit(p->arena, loc, slp);
    }
    return ident;
}
```

### Verification
After Fix 1, test if struct literals parse correctly. If not, the issue may be in how `parse_struct_lit_fields` handles the field parsing.

---

## Fix 3: Null Token Handling

**File**: `impl/parser/parser_expr.cpp`  
**Line**: 323-328  
**Severity**: HIGH - `null` literal not working

### Problem
The `null` case IS handled, but the error "unexpected token 'null'" suggests the token type is not `ZITH_TOKEN_NULL`. There's a debug fprintf that should print if this case is hit.

### Current Code (line 323-328):
```cpp
case ZITH_TOKEN_NULL: {
    fprintf(stderr, "DEBUG: matched ZITH_TOKEN_NULL case!\n");
    ZithLiteral lit;
    lit.kind = ZITH_LIT_NULL;
    lit.value.i64 = 0;
    return zith_ast_make_literal(p->arena, loc, lit);
}
```

### Fix:
1. Remove the debug fprintf
2. Verify that `null` is being tokenized as `ZITH_TOKEN_NULL` by checking the keyword table in `impl/lexer/keywords.cpp` line 46: `{"null", ZITH_TOKEN_NULL}`

The tokenization looks correct. The issue might be that `null` is being parsed in a context where it's not expected as a value. After Fix 1, this should work correctly.

### Cleaned up code:
```cpp
case ZITH_TOKEN_NULL: {
    ZithLiteral lit;
    lit.kind = ZITH_LIT_NULL;
    lit.value.i64 = 0;
    return zith_ast_make_literal(p->arena, loc, lit);
}
```

---

## Fix 4: Uninitialized Variable Type Mismatch

**File**: `impl/parser/parser_sema.cpp`  
**Line**: 402-410  
**Severity**: MEDIUM - False positive type errors

### Problem
When a variable is declared without an initializer (`var x: i32;`), it's marked as dirty. But the error "expected int, got void" suggests the type is being resolved incorrectly.

### Current Code (line 402-410):
```cpp
if (!var->initializer) {
    // No initializer
    if (var->binding == ZITH_BINDING_CONST) {
        parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                         "const declaration requires an initializer");
    }
    // Mark dirty — use declared type if available, otherwise Unknown
    Type t = declared.base != SemaType::Unknown ? declared : Type{};
    sema_define(ctx, name, t, var->binding, true);
}
```

### Analysis
The code looks correct - it uses the declared type. The error "expected int, got void" at line 52 suggests something else is wrong. Looking at the error message more carefully:

```
06_new_features.zith:52:9: error: type mismatch in 'x': expected int, got void
      var x: i32;
           ^
```

This error is coming from line 417-426 in the `else` branch (with initializer), not the no-initializer branch. But `var x: i32;` has NO initializer...

Wait, looking at the test file line 52-56:
```
52:     var x: i32;
53:     // let y = x;  // COMPILE ERROR: variable 'x' is not initialized
54:     x = 42;  // assignment clears the dirty flag
55:     let y = x;  // OK now
56:     return y;
```

The error might be from the assignment `x = 42;` on line 54. Let me check the assignment handling in sema_expr (line 752-807).

Actually, the error message says "type mismatch in 'x': expected int, got void". This is from line 424:
```cpp
snprintf(buf, sizeof(buf), "type mismatch in '%s': expected %s, got %s", name.c_str(), exp, got);
```

This is in the var_decl handling with initializer. But `var x: i32;` has no initializer... unless the parser is incorrectly picking up something as an initializer.

### Root Cause Hypothesis
The SCAN mode variable declaration parsing (parser_decl.cpp:252-261) skips expressions:
```cpp
if (parser_match(p, ZITH_TOKEN_ASSIGNMENT) || parser_match(p, ZITH_TOKEN_DECLARATION)) {
    if (p->mode == ZITH_MODE_SCAN) {
        // SCAN mode: skip the expression to avoid parsing dependencies
        while (!parser_check(p, ZITH_TOKEN_SEMICOLON) && !parser_check(p, ZITH_TOKEN_COMMA) &&
               !parser_is_at_end(p))
            parser_advance(p);
    } else {
        init = parser_parse_expression(p);
    }
}
```

But in PARSE/EXPAND mode, the initializer is parsed. If the parser is incorrectly treating something after the semicolon as an initializer, that would cause this error.

### Fix
After Fix 1 is applied, this error should be resolved. If not, additional debugging is needed to trace where the "void" type is coming from.

---

## Fix 5: Struct Type Registration Order

**File**: `impl/parser/parser_sema.cpp`  
**Line**: 1190-1206  
**Severity**: MEDIUM - Structs must be registered before use

### Problem
Struct definitions are registered in `sema_run` during the first pass (lines 1190-1206), before function bodies are analyzed. This should be correct, but verify that `Point` and `Vec3` are being registered.

### Current Code (line 1190-1206):
```cpp
if (decl->type == ZITH_NODE_STRUCT_DECL) {
    auto *sd = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    if (!sd) continue;
    StructDef def;
    def.name = std::string(sd->name, sd->name_len);
    for (size_t fi = 0; fi < sd->field_count; ++fi) {
        auto *field_node = sd->fields[fi];
        if (!field_node || field_node->type != ZITH_NODE_FIELD) continue;
        auto *fp = static_cast<ZithFieldPayload *>(field_node->data.list.ptr);
        if (!fp) continue;
        StructField sf;
        sf.name = std::string(fp->name, fp->name_len);
        sf.type = sema_type_from_node(fp->type_node, &ctx);
        def.fields.push_back(std::move(sf));
    }
    ctx.structs[def.name] = std::move(def);
    continue;
}
```

This looks correct. The structs should be registered before the function bodies are analyzed (lines 1210-1241).

---

## Testing Plan

After applying fixes:

1. **Build the project**:
   ```bash
   cd scripts/cmake-build-debug
   cmake --build .
   ```

2. **Run the test**:
   ```bash
   ./zith ../examples/06_new_features.zith
   ```

3. **Expected output**: No errors, or significantly reduced error count

4. **Verify each feature**:
   - Binding mutability (let/var/const)
   - Dirty variable detection
   - Type prohibition (?void = null)
   - Struct literals (tagged, positional, mixed)

---

## Summary of Changes

| Fix | File | Lines | Impact |
|-----|------|-------|--------|
| 1 | `impl/parser/parser_expr.cpp` | 368-369 | Fixes 6+ semicolon errors |
| 2 | `impl/parser/parser_expr.cpp` | 276-286 | Verify struct literals work after Fix 1 |
| 3 | `impl/parser/parser_expr.cpp` | 323-328 | Clean up null handling, remove debug |
| 4 | `impl/parser/parser_sema.cpp` | 402-410 | May be resolved by Fix 1 |
| 5 | `impl/parser/parser_sema.cpp` | 1190-1206 | Verify struct registration order |

The **primary fix** is Fix 1 - adding statement terminator checks to `parse_expr_bp`. This should resolve the majority of cascading errors.
