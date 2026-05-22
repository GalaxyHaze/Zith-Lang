# Fix `register_module` evaluation-order use-after-move

## Status: COMPLETED

## Target
`impl/import/module_registry.hpp` — one-line fix in `register_module()` causing module to be stored under empty key on C++17+.

## Background
Running `zith check examples/dev/small.zith` crashes with `STATUS_HEAP_CORRUPTION` (0xC0000374). Output shows `"  Module:"` with empty name — the module was registered under key `""` instead of `"small"`.

## Root Cause

### `register_module` — C++17 evaluation order bug
**File:** `module_registry.hpp:229`

```cpp
modules_[mod.name()] = std::make_shared<Module>(std::move(mod));
```

On C++17+, the RHS `std::make_shared<Module>(std::move(mod))` is sequenced before the LHS subscript `modules_[mod.name()]`. By the time `mod.name()` executes, `mod` has been moved-from, yielding an empty string. The module is stored under key `""`.

**Downstream effect:** Every `ensure_parser_module()` call → `get_module("small")` → `nullptr` (stored under `""`), so all symbol registrations silently fail. The module exists but has zero symbols. The heap corruption is a secondary symptom of repeated create/destroy of Modules during the scan loop.

## Fix

```cpp
bool register_module(Module mod) {
    std::string mod_name = mod.name();
    if (modules_.contains(mod_name)) {
        return false;
    }
    modules_[std::move(mod_name)] = std::make_shared<Module>(std::move(mod));
    return true;
}
```

Capture `mod.name()` by value **before** the move, then use the copy as the map key.

## Risk
Low — purely mechanical change, no semantic difference before C++17.
