# Code Deduplication Plan

## Status: COMPLETED

## Target
`impl/parser/` — deduplicate repeated patterns across parser files.

## Items

### 1. Ownership Switch Helper (parallel)
**Files:** `parser_decl.cpp:619-629, 634-644, 678-688`, `parser_expr.cpp:225-233`
**Severity:** 4 identical switch statements (~24 lines)
**Solution:** Extract to `parser_ownership_from_node(const ZithNode*)` in `parser_utils.cpp`

### 2. Tuple/Pack Creation Helper (parallel)
**Files:** `parser_expr.cpp:110-129, 299-318`
**Severity:** ~20 lines duplicate
**Solution:** Extract parameterized helper `parser_make_list_node(Parser*, ZithNodeType, fn_ptr, msg)` in `parser_utils.cpp`

### 3. Block Parsing Helper (parallel)
**Files:** `parser_decl.cpp:415-439`
**Severity:** ~15 lines duplicate (2 nearly-identical functions)
**Solution:** Extract `parser_parse_block_body(Parser*, bool expect_brace, ZithSourceLoc)` in `parser_decl.cpp`

### 4. EOF Token Deduplication (parallel)
**Files:** `parser_utils.cpp:57, 64`
**Severity:** 2 identical static EOF tokens
**Solution:** Move to anonymous namespace at file scope

### 5. Node Init via zith_ast_make_* (parallel)
**Files:** `parser_expr.cpp:45-74, 310-317`
**Severity:** Low (idiomatic, already partially abstracted)
**Solution:** Expand `zith_ast_make_*` family in `impl/ast/ast.cpp`

### 6. ArenaList Collection Helper (deferred)
**Files:** All `impl/parser/*.cpp` — 15+ occurrences
**Severity:** Medium (~40 lines duplicate)
**Solution:** Template helper `parser_collect_list()` in `parser_utils.cpp` (requires C++ template care)
**Note:** Lower priority — pattern already well-abstracted by ArenaList

## Implementation Order
1. Item 1 (Ownership switch)
2. Item 2 (Tuple/Pack)
3. Item 3 (Block parsing)
4. Item 4 (EOF token)
5. Item 5 (Node init)
6. Item 6 (ArenaList — deferred)