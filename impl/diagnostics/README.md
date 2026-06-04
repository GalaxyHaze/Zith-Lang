# Diagnostics System

Error and warning reporting with source location tracking.

## Files

```
diagnostics/
├── diagnostics.hpp  # Headers and C++ wrapper
└── diagnostics.cpp  # Implementation
```

## Purpose

- Centralized error/warning reporting
- Source location tracking (line, column)
- Note and info messages for context

## Severity Levels

| Level | Meaning | Exit Code |
|-------|---------|-----------|
| `ERROR` | Compilation failed | non-zero |
| `WARNING` | Possible issue | 0 |
| `NOTE` | Additional context | 0 |
| `INFO` | Debugging info | 0 |

## Data Structures

```cpp
typedef struct ZithDiagnostic {
    const char *message;
    ZithSourceLoc loc;
    ZithDiagSeverity severity;
} ZithDiagnostic;

typedef struct ZithDiagList {
    ZithDiagnostic *items;
    size_t count;
    size_t capacity;
} ZithDiagList;
```

## C API

```c
// Create a diagnostic list
void zith_diag_init(ZithDiagList *diags, ZithArena *arena);

// Emit an error
void zith_diag_error(ZithDiagList *diags, ZithSourceLoc loc, const char *msg);

// Emit a warning
void zith_diag_warning(ZithDiagList *diags, ZithSourceLoc loc, const char *msg);

// Print all diagnostics with source context
void zith_diag_print_all(const ZithDiagList *diags,
                         const char *source, size_t source_len,
                         const char *filename);
```

## C++ Wrapper

```cpp
class DiagManager {
public:
    void error(ZithSourceLoc loc, const char *msg);
    void warning(ZithSourceLoc loc, const char *msg);
    void note(ZithSourceLoc loc, const char *msg);
    
    bool had_error() const;
    const ZithDiagList& diagnostics() const;
};
```

## Usage

```cpp
DiagManager mgr;
mgr.error({5, 10}, "undefined identifier '%s'", name);
if (mgr.had_error()) {
    return nullptr;
}
```

## Integration

- Lexer reports tokenization errors
- Parser reports syntax errors (with panic mode recovery)
- SEMA reports type errors and undefined symbols
- CLI layer calls `zith_diag_print_all()` to output

## See Also

- `parser/parser_utils.cpp` — Error reporting in parser
- `include/zith/zith.hpp` — Source location types