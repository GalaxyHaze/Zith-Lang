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

### 4. Globals → ParseContext Struct ✅ COMPLETED
**File:** `impl/parser/parser.cpp`, `impl/parser/parser_context.hpp`
**Changes:**
- Added `std::unique_ptr<DiagManager> diag_manager` to `ParseContext` struct
- `zith_parse_with_source()` transfers DiagManager ownership to `tls_parse_ctx.diag_manager`
- `parser_destroy()` no longer deletes DiagManager (ownership transferred)
- Eliminates intentional DiagManager leak

### 5. TypeChecker Extraction ✅ COMPLETED
**Files:** `impl/parser/type_checker.hpp`, `impl/parser/type_checker.cpp`

**Completed:**
- Created `impl/parser/type_checker.cpp` with full implementation:
  - `TypeChecker::TypeChecker(TypeContext&, DiagManager*)` constructor
  - `TypeChecker::get_type(ZithNode*, Type)` — main entry with node-type dispatch
  - `TypeChecker::type_from_node(const ZithNode*)` — type resolution from AST nodes
  - `TypeChecker::is_assignable(const Type&, const Type&)` — type compatibility
  - `TypeChecker::is_coercible(const Type&, const Type&)` — implicit coercion rules
  - `TypeChecker::is_exact_match(const Type&, const Type&)` — exact type equality
  - `TypeChecker::check_struct_literal(ZithNode*, const Type&)` — struct literal validation
  - `TypeChecker::check_assignment(...)` — assignment type checking
  - `TypeChecker::emit_error(...)` — diagnostic emission via v2 system
  - `TypeChecker::base_type_name(SemaType)` — type name stringification
- Added `type_checker.cpp` to CMakeLists.txt `PARSER_CPP_SOURCES`

## Dependencies
- Items 1-5 are COMPLETED

## Implementation Order
1. Item 1 (thread_local globals) ✅ COMPLETED
2. Items 2-3 ✅ COMPLETED
3. Item 4 (ParseContext struct) ✅ COMPLETED
4. Item 5 (TypeChecker extraction) ✅ COMPLETED