# Type Representation v2 — Plan

## Pipeline (corrected)

```
spec:  SCAN → SEMA → SOLVE → NRA → HIR → MIR → codegen
code:  lex   sema   SOLVE   NRA   hir   mir
```

- **Sema**: type-checks generic bodies using `TypeGenericParam` + constraints.
- **Solve** (new stage): generic instantiation, constraint checking, monomorphization, comptime.
- **NRA**: borrow checker (future).
- **HIR** = `zir/hir/` (fully resolved after solve + NRA).
- **ZIR** = `zir/mir/` (the MIR step).

**`TypeIncomplete`** is the bridge: sema creates it for types that depend on generic/comptime values (e.g. `Vec<T>` inside `fn foo[T]`). It stores the base type + args (which may contain `TypeGenericParam`). The solver monomorphizes `TypeIncomplete` into a concrete type when all args become concrete.

After solve, `TypeGenericParam`, `TypeIncomplete`, and constraint metadata are gone — every type is a concrete `TypeId`.

---

## 1. TypeExpr AST (`src/ast/type-expr.hpp`)

Parsed from annotations instead of `scan_skip_type_expr`. `TypeExprId` on `AstBuilder`.

```cpp
enum class BuiltinType : uint8_t {
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    Bool, Char, Void, Never,
    Unknown, Invalid, Opaque,
};

struct TypePath        { DynArray<string_view> segments; };  // Vec, std.vec.Vec
struct TypeBuiltin     { BuiltinType kind; };                 // i32, bool, unknown
struct TypePtr         { TypeExprId pointee; bool is_mut; types::OwnershipKind ownership; };
struct TypeSlice       { TypeExprId elem; };                  // []T
struct TypeArray       { TypeExprId elem; TypeExprId count; };// [N]T, [_]T
struct TypeFnExpr      { DynArray<TypeExprId> params; TypeExprId ret; };  // fn(i32): bool
struct TypeOptional    { TypeExprId inner; };                  // ?T
struct TypeFailable    { TypeExprId inner; TypeExprId error; };// T!, T!E
struct TypeApp         { TypeExprId base; DynArray<TypeExprId> args; };  // Vec<i32>
struct TypePackMember  { string_view name; TypeExprId type; };
struct TypePack        { DynArray<TypePackMember> members; };  // |name: T, U|
struct TypeSum         { DynArray<TypeExprId> members; };      // i32 or f64
struct TypeInfer       {};                                      // _ (inferred)
struct TypeGenericParamRef { string_view name; };                // T (inside generic body)

using TypeExprNode = variant<...>;
```

### Grammar

```
typeExpr  → constraint ('+' constraint)*          // AND: T: Printable + Integer
constraint → orExpr
orExpr    → primary ('or' primary)*               // OR: i32 or f64
primary   → pack | fnType | ptrExpr | slice | array | optional | failable
          | path | builtin | unknown | invalid | opaque | '_' | '(' typeExpr ')'
pack      → '|' member (',' member)* '|'
fnType    → 'fn' '(' params ')' (':' typeExpr)?
```

---

## 2. Declaration changes (`src/ast/ast-nodes.hpp`)

```cpp
struct FnParam {
    string_view name;
    TypeExprId type = kInvalidTypeExpr;   // kInvalidTypeExpr = inferred
};

struct GenericParam {
    string_view name;
    DynArray<TypeExprId> bounds;   // T: Printable + Integer — each lowered to TypeId
};

// Updated FnDeclNode:
struct FnDeclNode {
    string_view name;
    DynArray<GenericParam> generic_params;
    DynArray<FnParam> params;
    TypeExprId return_type;       // kInvalidTypeExpr = inferred
    ExprId body;
};

// Updated LetNode:
struct LetNode {
    bool mut = false;
    TypeExprId type_annot;
    DynArray<string_view> names;  // supports [x, y, z]: T destructuring
    ExprId init;
};

// StructDeclNode, EnumDeclNode, UnionDeclNode — add generic_params
```

---

## 3. New TypeData kinds (`src/types/type-kind.hpp`)

```cpp
enum class OwnershipKind : uint8_t {
    Default, Unique, Share, Lend, View, Belong
};

struct TypePtr {
    TypeId pointee;
    bool is_mut;
    OwnershipKind ownership;
};
```

### New types added to TypeData

| Kind | Struct | Purpose |
|------|--------|---------|
| `TypeUnknown` | `{}` | Valid but unresolved (compiler-internal) |
| `TypeSlice` | `{ TypeId elem }` | `[]T` — fat pointer |
| `TypeEnum` | `{ TypeId def_id }` | Nominal enum |
| `TypeUnion` | `{ TypeId def_id }` | Nominal runtime tagged union |
| `TypePack` | `{ const TypeId \*members; const string_view \*names; size_t count }` | Tuple/pack type, arena-backed |
| `TypeSum` | `{ const TypeId \*members; size_t count }` | `i32 or f64` — compile-time type-level sum, arena-backed |
| `TypeGenericParam` | `{ uint32_t decl_id; uint32_t param_index }` | `T` inside a generic body |
| `TypeIncomplete` | `{ TypeId base; const TypeId \*args; size_t arg_count }` | `Vec\<T\>` — applied type with unresolved args, arena-backed |

**NOT in TypeData:**
- `TypeString` — string is `[]char` (stdlib)
- No other type needs to be excluded

### TypeKind enum additions

```cpp
enum class TypeKind : uint8_t {
    Error, Never, Void, Bool, Char,      // 0–4
    Int, Float, Ptr, Array,              // 5–8
    Struct, Fn, TypeVar,                 // 9–11
    Optional, Failable, Opaque,          // 12–14
    Unknown,                             // 15
    Slice,                               // 16
    Enum,                                // 17
    Union,                               // 18
    Pack,                                // 19
    Sum,                                 // 20
    GenericParam,                        // 21
    Incomplete,                          // 22
};
```

---

## 4. Generics metadata

**Not in TypeData.** `TypeGenericParam` is just an index. Declaration-specific metadata lives in a new `GenericDef` table on `SemaContext`:

```cpp
struct GenericParamDef {
    string_view name;
    memory::DynArray<TypeId> bounds;   // lowered TypeExpr → concrete TypeIds
};
```

The `decl_id` in `TypeGenericParam` maps to the AST `DeclId`. Solve stage maps `(decl_id, param_index)` → `GenericParamDef`.

---

## 5. Type lowering (`src/types/type-lower.hpp`)

```cpp
class TypeLower {
    TypeIntern &intern_;
    HashMap<string_view, TypeId> *generic_ctx_;  // name → TypeGenericParam mapping
public:
    TypeId lower(TypeExprId id);
    void setGenericContext(...);
};
```

---

## 6. Constraint checking (Solve stage)

```
bound is TypeSum     → arg_type ∈ bound.members
bound is TypeTrait   → arg_type implements trait  (TBD, error for now)
bound is concrete    → arg_type == bound
```

---

## 7. TypeWalker utility (`src/types/type-walker.hpp`)

```cpp
void walkSubTypes(TypeKind kind, const TypeData &data,
                  function_ref<void(TypeId)> fn);
```

Reduces duplication across unify, typeDataEqual, occurs, isAssignable, isCoercible.

---

## 8. Solve stage (`src/solve/`)

```cpp
class Solver {
    // Takes sema output, produces fully-resolved HIR
    bool run();
};
```

Handles: generic instantiation, constraint checking, monomorphization, future comptime.

---

## 9. Implementation order

| Step | What | Risk |
|------|------|------|
| 1 | `TypeWalker` utility | Low |
| 2 | `TypePtr` with `is_mut` + `OwnershipKind` | Low |
| 3 | Add new kinds: Unknown, Slice, Enum, Union, Pack, Sum, GenericParam | Medium |
| 4 | TypeExpr AST + parser changes | Medium |
| 5 | TypeExpr → TypeId lowering pass | Medium |
| 6 | Update AST declarations (FnParam, GenericParam, LetNode) | Low |
| 7 | Wire type checking into sema (`unifier.unify()` calls) | High |
| 8 | Solve stage | High |
| 9 | Solve into pipeline | Medium |
| 10 | Hash-consing dedup | Low |
