# Module System Consolidation Plan

## Status: IMPLEMENTED

## Target
`impl/import/` — four files with overlapping responsibilities, duplicate definitions, inconsistent patterns.

## Files
| File | Responsibility |
|------|----------------|
| `import.hpp` | RAII container for symbols organized by visibility |
| `symbol_table.hpp` | Global symbol resolution based on Import (v1) |
| `symbol_resolver.hpp` | Symbol resolution via ModuleRegistry (v2) |
| `module_registry.hpp` | Central module management |

## Items

### 1. Rename Conflicting SymbolResolution Classes (parallel)
**Files:** `symbol_table.hpp`, `symbol_resolver.hpp`
**Severity:** Two classes with identical name but different types
**Solution:** Rename `SymbolResolver::SymbolResolution` → `ResolvedSymbol` (or `SymbolResolutionResult`)
**Risk:** Low

### 2. Merge Duplicate TypeSignature Definition (parallel)
**Files:** `symbol_table.hpp`, `symbol_resolver.hpp`
**Severity:** Identical struct defined in two places
**Solution:** Move to `module_registry.hpp` (since SymbolEntry uses it), remove from `symbol_table.hpp`
**Risk:** Low

### 3. Fix SymbolTable::unindex_import() No-Op (parallel)
**File:** `symbol_table.hpp`
**Severity:** Index never cleaned up on unregister — clear() must rebuild everything
**Solution:** Implement proper removal from `symbols_by_name_` map and the 9-vector visibility arrays
**Risk:** Low

### 4. Document/Consolidate v1 vs v2 Resolution
**Files:** `symbol_table.hpp`, `symbol_resolver.hpp`
**Severity:** Two independent resolution systems with no clear winner
**Decision:** Keep both — v1 is the ABI/NIF layer, v2 is the current implementation. Both files now carry header comments clarifying their role.
**Action:** Added v1/v2 labels to file headers, no code changes needed
**Risk:** Medium (documented, mitigated)

### 5. Standardize clear() Responsibility Hierarchy (parallel)
**Files:** `module_registry.hpp`, `symbol_table.hpp`, `symbol_resolver.hpp`
**Severity:** Three clear() methods with different scopes — confusing and error-prone
**Solution:** Define clear hierarchy:
- `ModuleRegistry::clear()` → reset modules only
- `SymbolTable::clear()` → reset imports and index
- `SymbolResolver::clear()` → reset aliases only (delegate to ModuleRegistry)
- Top-level `reset_all()` → calls all three
**Risk:** Low

### 6. Remove Redundant O(n^2) Conflict Detection (parallel)
**File:** `symbol_resolver.hpp`
**Severity:** `detect_all_conflicts()` and `validate_module()` both do O(n^2) scans; `Module::add_symbol()` already prevents duplicates
**Solution:**
- Remove `detect_all_conflicts()` entirely
- Simplify `validate_module()` to only semantic overload checks
- Keep `detect_duplicate()` for single-symbol validation
**Risk:** Low

### 7. Singleton CRTP Base (parallel, low priority)
**Files:** All three singleton files
**Severity:** Low (code style only)
**Solution:**
```cpp
template<typename T> struct Singleton {
    static T& instance() { static T inst; return inst; }
protected: Singleton() = default; friend T;
};
struct ModuleRegistry : Singleton<ModuleRegistry> { ... };
```
**Risk:** None

## Implementation Order
1. Item 1 (Rename SymbolResolution) — 5 min
2. Item 2 (Merge TypeSignature) — 5 min
3. Item 3 (Fix unindex_import) — 15 min
4. Item 5 (Standardize clear) — 15 min
5. Item 6 (Remove O(n^2)) — 15 min
6. Item 7 (Singleton CRTP) — 10 min
7. Item 4 (v1/v2 decision) — requires user decision

## Dependencies
All parallel except Item 4 (requires understanding of Item 1-3 changes first)