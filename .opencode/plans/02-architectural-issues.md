# Architectural Issues Plan

## Status: IN PROGRESS

## Items

### 1. Globals → thread_local (parallel)
**File:** `impl/parser/parser.cpp:13-19`
**Severity:** 5 globals (not thread-safe, DiagManager leak)
**Solution:** Convert all `static` globals to `static thread_local` — instant thread-safety win, minimal code change
**Risk:** Low

### 2. Diagnostics v1 Internal Deprecation ✅ COMPLETED
**Files:** `impl/diagnostics/diagnostics.hpp`, `impl/diagnostics/diagnostics.cpp`
**Changes:**
- Replaced internal `emit()` with v2 fluent builder (`build().with_raw_message().with_span().emit(bag_)`)
- Added new templated methods: `emit_error()`, `emit_warning()`, `emit_note()` using v2 builder
- Changed `list()` to return by value (builds on-demand) instead of caching reference
- Removed `legacy_stale_` flag and `rebuild_legacy_list()` (replaced with `build_legacy_list()`)
- All v1 legacy internal methods now delegate to v2 DiagnosticBag

### 3. Diagnostics v1 C API Cleanup ✅ COMPLETED
**Files:** `include/zith/diagnostics.h`, `impl/diagnostics/diagnostics.cpp`
**Changes:**
- Marked `zith_diag_print_all()` section as DEPRECATED with comment
- Moved `ZithSourceSpan`/`ZithFileId` v2 types to "internal, used by C++ implementation" section
- Updated header comment from "v2 (New) Types — richer span, file tracking" to "v2 (New) Types — internal, used by C++ implementation"

### 4. Globals → ParseContext Struct (pending)
**File:** `impl/parser/parser.cpp`
**Severity:** Intentionally leaked DiagManager, hidden LSP coupling
**Solution:**
- Create `struct ParseContext { vector<ZithNode*> imported_decls; int depth; bool import_loaded; unique_ptr<DiagManager> diag_manager; }`
- Thread `ParseContext&` through `run_parser_phase()` / `expand_unbody()` OR make `Parser` own it
- Return `ParseResult { root, DiagManager*, ZithDiagList }` to callers
**Risk:** Medium — requires threading context through all parser functions
**Alternative:** Use `thread_local` + RAII guard (simpler, defer full DI)

### 5. TypeChecker Extraction (pending — PARTIAL START)
**File:** `impl/parser/parser_sema.cpp` (1245 lines)
**Severity:** Type checking mixed with AST traversal in SemaContext

**Completed:**
- Created `impl/parser/type_checker.hpp` with:
  - `TypeContext` class (holds `functions`, `structs` maps)
  - `TypeChecker` class skeleton with method signatures
  - Helper functions: `base_type_name()`, `is_exact_match()`, `type_from_node()`

**Remaining to implement:**
1. Create `impl/parser/type_checker.cpp` with:
   - `TypeChecker::check(ZithNode*)` main entry that dispatches to node-type handlers
   - `TypeChecker::get_type()` — expression type inference
   - `TypeChecker::check_assignment()` — assignment validation
   - `TypeChecker::is_assignable()` / `is_coercible()` — type compatibility
   - `validate_call_ownership()` — ownership rule enforcement

2. Refactor `parser_sema.cpp`:
   - Make `SemaContext` delegate to `TypeContext` for struct/function lookup
   - Replace inline type-checking logic in `sema_expr()` with `TypeChecker::get_type()` calls
   - Replace inline assignment logic with `TypeChecker::check_assignment()`
   - Remove duplicate `base_type_name()`, `type_to_string()`, `is_exact_match()`, `is_assignable()`, `is_coercible()`, `sema_assignable()` from anonymous namespace
   - Simplify `sema_run()` to orchestrate: parse → type-check → report

3. Update `sema_push_scope()` / `sema_pop_scope()` / `sema_define()` to work with `TypeContext`:
   - Move scope stack into `TypeContext` or keep in `SemaContext` but call `TypeChecker` for checks

4. Wire up `DiagManager*` into `TypeChecker` constructor for diagnostic emission

**Note:** High-risk refactoring — consider keeping SemaContext for purely syntactic transformations and moving all type-checking logic to TypeChecker class.

## Dependencies
- Item 4 depends on Item 1 (thread_local baseline first)
- Items 2-3 are COMPLETED
- Item 5 is independent but benefits from Items 1-4

## Implementation Order
1. Item 1 (thread_local globals) — 20 min
2. Items 2-3 ✅ COMPLETED
3. Item 4 (ParseContext struct) — 2-3 hours
4. Item 5 (TypeChecker extraction) — full day