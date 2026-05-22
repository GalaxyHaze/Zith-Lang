// impl/parser/parser_sema.cpp — Semantic Analysis Phase
//
// Refactored from parser.cpp. Handles type checking, name resolution,
// scope management, and control-flow analysis.
#include "memory/arena.hpp"
#include "zith/parser.h"
#include "zith/typesystem.hpp"
#include "import/import.hpp"
#include "import/module_registry.hpp"
#include "parser_context.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using zith::ArenaList;
using zith::Type;
using zith::SemaType;

namespace {

struct VarEntry {
    Type type;
    ZithBindingKind binding = ZITH_BINDING_LET;
    bool is_dirty = false;
};

struct SemaScope {
    std::unordered_map<std::string, VarEntry> vars;
};

struct StructField {
    std::string name;
    Type type;
};

struct StructDef {
    std::string name;
    std::vector<StructField> fields;
};

struct SemaContext {
    Parser *p;
    std::unordered_map<std::string, std::vector<ZithFuncPayload *>> functions;
    std::unordered_set<std::string> imported_roots;
    std::unordered_map<std::string, StructDef> structs;
    Type current_return{};
    std::vector<SemaScope> scopes;
    std::unordered_map<ZithFuncPayload *, Type> inferred_returns;
    std::unordered_set<std::string> analyzing_fns;
};

static std::string ident_name(const ZithNode *n) {
    if (!n || n->type != ZITH_NODE_IDENTIFIER || !n->data.ident.str)
        return {};
    return std::string(n->data.ident.str, n->data.ident.len);
}

static const char *base_type_name(SemaType t) {
    switch (t) {
    case SemaType::Void:
        return "void";
    case SemaType::Char:
        return "char";
    case SemaType::Int:
        return "int";
    case SemaType::Float:
        return "float";
    case SemaType::String:
        return "string";
    case SemaType::Bool:
        return "bool";
    case SemaType::Opaque:
        return "opaque";
    case SemaType::Invalid:
        return "invalid";
    case SemaType::VoidPtr:
        return "voidPtr";
    case SemaType::Module:
        return "module";
    case SemaType::Struct:
        return "struct";
    case SemaType::Array:
        return "array";
    case SemaType::Slice:
        return "slice";
    default:
        return "unknown";
    }
}

static void type_to_string(const Type &t, char *out, size_t out_len) {
    if (!out || out_len == 0)
        return;
    
    if ((t.base == SemaType::Array || t.base == SemaType::Slice) && t.element_type) {
        char elem_buf[128] = {0};
        type_to_string(*t.element_type, elem_buf, sizeof(elem_buf));
        
        if (t.base == SemaType::Array)
            snprintf(out, out_len, "%s%s%s[%zu]%s", t.optional ? "?" : "", t.failable ? "!" : "", 
                     t.optional || t.failable ? "" : "", t.array_size, elem_buf);
        else
            snprintf(out, out_len, "%s%s%s[]%s", t.optional ? "?" : "", t.failable ? "!" : "",
                     t.optional || t.failable ? "" : "", elem_buf);
        return;
    }
    
    const char *name = t.type_name ? t.type_name : base_type_name(t.base);
    snprintf(out, out_len, "%s%s%s%s", t.optional ? "?" : "", t.failable ? "!" : "", name, "");
}

static Type sema_type_from_node(const ZithNode *n, const SemaContext *ctx = nullptr) {
    if (!n)
        return {};
    if (n->type == ZITH_NODE_UNARY_OP) {
        Type inner     = sema_type_from_node(n->data.kids.a, ctx);
        const ZithTokenType op = static_cast<ZithTokenType>(n->data.list.len & 0xFFFF);
        if (op == ZITH_TOKEN_QUESTION)
            inner.optional = true;
        else if (op == ZITH_TOKEN_BANG)
            inner.failable = true;
        return inner;
    }
    // Ownership type nodes — extract ownership and resolve inner type
    {
        ZithOwnership own = ZITH_OWN_DEFAULT;
        if (n->type == ZITH_NODE_TYPE_UNIQUE)
            own = ZITH_OWN_UNIQUE;
        else if (n->type == ZITH_NODE_TYPE_SHARED)
            own = ZITH_OWN_SHARED;
        else if (n->type == ZITH_NODE_TYPE_VIEW)
            own = ZITH_OWN_VIEW;
        else if (n->type == ZITH_NODE_TYPE_LEND)
            own = ZITH_OWN_LEND;
        else if (n->type == ZITH_NODE_TYPE_PACK || n->type == ZITH_NODE_TYPE_EXTENSION)
            own = ZITH_OWN_EXTENSION;

        if (own != ZITH_OWN_DEFAULT) {
            Type inner = sema_type_from_node(n->data.kids.a, ctx);
            inner.ownership = own;
            return inner;
        }
    }
    // Pointer type node — raw pointer (void* semantics)
    if (n->type == ZITH_NODE_TYPE_POINTER) {
        Type inner = sema_type_from_node(n->data.kids.a, ctx);
        if (inner.base == SemaType::Opaque || inner.base == SemaType::Unknown)
            return {SemaType::VoidPtr, false, false, ZITH_OWN_DEFAULT};
        return {SemaType::VoidPtr, false, false, ZITH_OWN_DEFAULT};
    }
    // Array type node — [N]T
    if (n->type == ZITH_NODE_TYPE_ARRAY) {
        ZithNode *elem = static_cast<ZithNode *>(n->data.list.ptr);
        if (!elem) {
            return {};
        }
        Type inner = sema_type_from_node(elem, ctx);
        if (inner.base != SemaType::Unknown) {
            ZithArena *arena = ctx ? ctx->p->arena : nullptr;
            Type arr;
            arr.base = SemaType::Array;
            if (arena) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena, sizeof(Type)));
                if (et) { *et = inner; arr.element_type = et; }
            }
            arr.array_size = n->data.list.len;
            return arr;
        }
        return {};
    }
    // Slice type node — []T
    if (n->type == ZITH_NODE_TYPE_SLICE) {
        ZithNode *elem = static_cast<ZithNode *>(n->data.list.ptr);
        if (!elem) {
            return {};
        }
        Type inner = sema_type_from_node(elem, ctx);
        if (inner.base != SemaType::Unknown) {
            ZithArena *arena = ctx ? ctx->p->arena : nullptr;
            Type slice;
            slice.base = SemaType::Slice;
            if (arena) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena, sizeof(Type)));
                if (et) { *et = inner; slice.element_type = et; }
            }
            slice.array_size = 0;
            return slice;
        }
        return {};
    }
    if (n->type == ZITH_NODE_IDENTIFIER && n->data.ident.str) {
        const std::string name(n->data.ident.str, n->data.ident.len);
        if (name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
            name == "u8" || name == "u16" || name == "u32" || name == "u64")
            return {SemaType::Int, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "f32" || name == "f64" || name == "float")
            return {SemaType::Float, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "char")
            return {SemaType::Char, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "string")
            return {SemaType::String, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "bool")
            return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "void")
            return {SemaType::Void, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "unknown")
            return {SemaType::Unknown, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "opaque")
            return {SemaType::Opaque, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "voidPtr")
            return {SemaType::VoidPtr, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (name == "invalid")
            return {SemaType::Invalid, false, false, ZITH_OWN_DEFAULT, nullptr, n->data.ident.str};
        if (ctx) {
            auto it = ctx->structs.find(name);
            if (it != ctx->structs.end())
                return {SemaType::Struct, false, false, ZITH_OWN_DEFAULT, it->second.name.c_str(), nullptr};
        }
        return {};
    }
    return {};
}

static bool is_exact_match(const Type &lhs, const Type &rhs) {
    return zith::type_match(lhs, rhs);
}

static bool is_assignable(const Type &dst, const Type &src);

static bool is_coercible(const Type &dst, const Type &src) {
    if (dst.base == SemaType::Unknown || src.base == SemaType::Unknown)
        return true;
    if (dst.base == SemaType::Invalid || src.base == SemaType::Invalid)
        return true;
    if (dst.base == SemaType::Opaque || src.base == SemaType::Opaque)
        return true;
    if (dst.base == SemaType::VoidPtr || src.base == SemaType::VoidPtr)
        return true;
    if (src.base == SemaType::Void && src.optional && dst.optional)
        return true;
    if (dst.base == SemaType::Slice && src.base == SemaType::Array) {
        if (dst.array_size != 0)
            return false;
        if (!dst.element_type || !src.element_type)
            return false;
        return is_assignable(*dst.element_type, *src.element_type);
    }
    return false;
}

static bool is_assignable(const Type &dst, const Type &src) {
    if (is_exact_match(dst, src))
        return true;
    if (dst.base != src.base)
        return is_coercible(dst, src);
    if (dst.base == SemaType::Struct) {
        if (!dst.struct_name || !src.struct_name)
            return false;
        if (strcmp(dst.struct_name, src.struct_name) != 0)
            return false;
    }
    if (dst.base == SemaType::Array || dst.base == SemaType::Slice) {
        if (dst.base == SemaType::Array && dst.array_size != src.array_size)
            return false;
        if (!dst.element_type || !src.element_type)
            return false;
        if (!is_assignable(*dst.element_type, *src.element_type))
            return false;
    }
    if (dst.ownership != src.ownership) {
        if (src.ownership != ZITH_OWN_DEFAULT && dst.ownership != ZITH_OWN_DEFAULT)
            return false;
    }
    if (!dst.optional && src.optional)
        return false;
    if (!dst.failable && src.failable)
        return false;
    return true;
}

static bool sema_assignable(const Type &dst, const Type &src) {
    if (is_assignable(dst, src))
        return true;
    if (is_coercible(dst, src))
        return true;
    if (dst.base == SemaType::Unknown || src.base == SemaType::Unknown)
        return true;
    return false;
}

static void sema_push_scope(SemaContext &ctx) {
    ctx.scopes.push_back({});
}
static void sema_pop_scope(SemaContext &ctx) {
    if (!ctx.scopes.empty())
        ctx.scopes.pop_back();
}

static void sema_define(SemaContext &ctx, const std::string &name, Type t,
                        ZithBindingKind binding = ZITH_BINDING_LET, bool is_dirty = false) {
    if (ctx.scopes.empty())
        sema_push_scope(ctx);
    ctx.scopes.back().vars[name] = {t, binding, is_dirty};
}

static void sema_set_clean(SemaContext &ctx, const std::string &name, size_t skip_scopes = 0) {
    if (skip_scopes >= ctx.scopes.size())
        return;
    for (auto it = ctx.scopes.rbegin() + skip_scopes; it != ctx.scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) {
            found->second.is_dirty = false;
            return;
        }
    }
}

static const VarEntry *sema_lookup_var(const SemaContext &ctx, const std::string &name,
                                        size_t skip_scopes = 0, bool *found_out = nullptr) {
    if (skip_scopes >= ctx.scopes.size()) {
        if (found_out) *found_out = false;
        return nullptr;
    }
    for (auto it = ctx.scopes.rbegin() + skip_scopes; it != ctx.scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) {
            if (found_out) *found_out = true;
            return &found->second;
        }
    }
    if (found_out) *found_out = false;
    return nullptr;
}

static Type sema_lookup(const SemaContext &ctx, const std::string &name, size_t skip_scopes = 0,
                        bool *found_out = nullptr) {
    auto *ve = sema_lookup_var(ctx, name, skip_scopes, found_out);
    return ve ? ve->type : Type{};
}

/*static bool var_is_dirty(const SemaContext &ctx, const std::string &name) {
    bool found = false;
    auto *ve = sema_lookup_var(ctx, name, 0, &found);
    return found && ve->is_dirty;
}*/

static Type sema_expr(SemaContext &ctx, ZithNode *expr, Type expected_return = {});
static void validate_call_ownership(SemaContext &ctx, ZithCallPayload *call, ZithFuncPayload *fn);

Type sema_stmt(SemaContext &ctx, ZithNode *stmt) {
    if (!stmt)
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    switch (stmt->type) {
    case ZITH_NODE_DESTRUCTURE: {
        auto *p = static_cast<ZithDestructurePayload *>(stmt->data.list.ptr);
        if (!p)
            break;
        const Type declared = sema_type_from_node(p->type_node, &ctx);
        if (p->type_node && declared.base == SemaType::Unknown) {
            char buf[256];
            const char *type_str = p->type_node->data.ident.str;
            size_t type_len = p->type_node->data.ident.len;
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", static_cast<int>(type_len), type_str);
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
        }
        Type init_type = sema_expr(ctx, p->initializer, declared.base != SemaType::Unknown ? declared : Type{});
        for (size_t i = 0; i < p->count; ++i) {
            const std::string name(p->names[i], p->name_lens[i]);
            Type var_type = declared;
            // If initializer is a pack literal, extract element type
            if (p->initializer && p->initializer->type == ZITH_NODE_TUPLE_LIT && i < p->initializer->data.list.len) {
                auto **items = static_cast<ZithNode **>(p->initializer->data.list.ptr);
                if (items && items[i])
                    var_type = sema_expr(ctx, items[i]);
            } else if (p->initializer && p->initializer->type != ZITH_NODE_TUPLE_LIT) {
                // Scalar initializer — replicate to all vars
                var_type = init_type;
            }
            if (var_type.base == SemaType::Unknown)
                var_type = declared;
            bool is_dirty = !p->initializer;
            if (is_dirty && p->binding == ZITH_BINDING_CONST) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "const declaration requires an initializer");
            }
            sema_define(ctx, name, var_type, p->binding, is_dirty);
        }
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    }
    case ZITH_NODE_VAR_DECL: {
        auto *var = static_cast<ZithVarPayload *>(stmt->data.list.ptr);
        const std::string name(var->name, var->name_len);
        const Type declared = sema_type_from_node(var->type_node, &ctx);
        if (var->type_node && declared.base == SemaType::Unknown) {
            char buf[256];
            const char *type_str = var->type_node->data.ident.str;
            size_t type_len = var->type_node->data.ident.len;
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", static_cast<int>(type_len), type_str);
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
        }
        // Prohibit instantiating void, invalid, unknown
        if (var->type_node) {
            if (declared.base == SemaType::Void && !declared.optional) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "cannot instantiate type 'void'");
            } else if (declared.base == SemaType::Invalid) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "cannot instantiate type 'invalid'");
            } else if (declared.base == SemaType::Unknown) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "cannot instantiate type 'unknown'");
            }
        }
        if (!var->initializer) {
            // No initializer
            if (var->binding == ZITH_BINDING_CONST) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "const declaration requires an initializer");
            }
            // Mark dirty — use declared type if available, otherwise Unknown
            Type t = declared.base != SemaType::Unknown ? declared : Type{};
            sema_define(ctx, name, t, var->binding, true);
        } else {
            const Type init = sema_expr(ctx, var->initializer, declared.base != SemaType::Unknown ? declared : Type{});
            if (init.base == SemaType::Void && !init.optional) {
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                                 "expression has type void, cannot be used as a value");
            }
            if (declared.base != SemaType::Unknown && init.base != SemaType::Unknown &&
                !sema_assignable(declared, init)) {
                char buf[256];
                char exp[64] = {0};
                char got[64] = {0};
                type_to_string(declared, exp, sizeof(exp));
                type_to_string(init, got, sizeof(got));
                snprintf(buf, sizeof(buf), "type mismatch in '%s': expected %s, got %s", name.c_str(),
                         exp, got);
                parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
            }
            Type resolved = declared.base != SemaType::Unknown ? declared : init;
            sema_define(ctx, name, resolved, var->binding, false);
        }
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    }
    case ZITH_NODE_RETURN: {
        const Type ret = sema_expr(ctx, stmt->data.kids.a, ctx.current_return);
        if (ctx.current_return.base == SemaType::Void && stmt->data.kids.a) {
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR,
                             "void function cannot return a value");
        } else if (ctx.current_return.base != SemaType::Void &&
                   !sema_assignable(ctx.current_return, ret)) {
            char exp[64] = {0};
            char got[64] = {0};
            type_to_string(ctx.current_return, exp, sizeof(exp));
            type_to_string(ret, got, sizeof(got));
            char buf[256];
            snprintf(buf, sizeof(buf), "return type mismatch: expected %s, got %s", exp, got);
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
        }
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    }
    case ZITH_NODE_IF:
        sema_expr(ctx, stmt->data.kids.a);
        sema_stmt(ctx, stmt->data.kids.b);
        sema_stmt(ctx, stmt->data.kids.c);
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    case ZITH_NODE_FOR: {
        auto *f = static_cast<ZithForPayload *>(stmt->data.list.ptr);
        sema_push_scope(ctx);
        if (f) {
            if (f->iterator_var)
                sema_expr(ctx, f->iterator_var);
            if (f->iterable)
                sema_expr(ctx, f->iterable);
            if (f->init)
                sema_stmt(ctx, f->init);
            if (f->condition)
                sema_expr(ctx, f->condition);
            if (f->step)
                sema_expr(ctx, f->step);
            sema_stmt(ctx, f->body);
        }
        sema_pop_scope(ctx);
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    }
    case ZITH_NODE_BLOCK: {
        sema_push_scope(ctx);
        auto **stmts = static_cast<ZithNode **>(stmt->data.list.ptr);
        Type last_type = {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        for (size_t i = 0; i < stmt->data.list.len; ++i)
            last_type = sema_stmt(ctx, stmts[i]);
        sema_pop_scope(ctx);
        return last_type;
    }
    default:
        return sema_expr(ctx, stmt);
    }
}

static Type sema_expr(SemaContext &ctx, ZithNode *expr, Type expected_return) {
    if (!expr)
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    switch (expr->type) {
    case ZITH_NODE_LITERAL: {
        auto *lit = static_cast<ZithLiteral *>(expr->data.list.ptr);
        if (!lit)
            return {};
        switch (lit->kind) {
        case ZITH_LIT_INT:
        case ZITH_LIT_UINT:
            return {SemaType::Int, false, false, ZITH_OWN_DEFAULT};
        case ZITH_LIT_FLOAT:
            return {SemaType::Float, false, false, ZITH_OWN_DEFAULT};
        case ZITH_LIT_STRING: {
            Type char_type = {SemaType::Char, false, false, ZITH_OWN_DEFAULT};
            Type str;
            str.base = SemaType::Slice;
            auto *et = static_cast<Type *>(zith_arena_alloc(ctx.p->arena, sizeof(Type)));
            if (et) { *et = char_type; str.element_type = et; }
            str.array_size = 0;
            return str;
        }
        case ZITH_LIT_BOOL:
            return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT};
        case ZITH_LIT_CHAR:
            return {SemaType::Char, false, false, ZITH_OWN_DEFAULT};
        case ZITH_LIT_NULL:
            return {SemaType::Void, true, false, ZITH_OWN_DEFAULT};
        }
        return {};
    }
    case ZITH_NODE_IDENTIFIER: {
        const std::string name = ident_name(expr);
        if (name == "true" || name == "false")
            return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT, nullptr, nullptr};
        bool found = false;
        auto *ve = sema_lookup_var(ctx, name, 0, &found);
        if (!found && !name.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "undefined identifier '%s'", name.c_str());
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            return {};
        }
        if (found && ve && ve->is_dirty) {
            char buf[256];
            snprintf(buf, sizeof(buf), "variable '%s' is not initialized", name.c_str());
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
        }
        return ve ? ve->type : Type{};
    }
    case ZITH_NODE_ARRAY_LIT: {
        auto **items = static_cast<ZithNode **>(expr->data.list.ptr);
        size_t count = expr->data.list.len;
        
        if (count == 0) {
            if (expected_return.base == SemaType::Array || expected_return.base == SemaType::Slice) {
                Type arr = expected_return;
                arr.array_size = count;
                return arr;
            }
            Type arr;
            arr.base = SemaType::Array;
            auto *et = static_cast<Type *>(zith_arena_alloc(ctx.p->arena, sizeof(Type)));
            if (et) { *et = Type{SemaType::Int, false, false, ZITH_OWN_DEFAULT}; arr.element_type = et; }
            arr.array_size = 0;
            return arr;
        }
        
        Type elem_type = sema_expr(ctx, items[0]);
        for (size_t i = 1; i < count; ++i) {
            Type item_type = sema_expr(ctx, items[i]);
            if (elem_type.base != SemaType::Unknown && item_type.base != SemaType::Unknown &&
                elem_type.base != item_type.base) {
                char buf[256];
                char t1[64] = {0};
                char t2[64] = {0};
                type_to_string(elem_type, t1, sizeof(t1));
                type_to_string(item_type, t2, sizeof(t2));
                snprintf(buf, sizeof(buf), "array literal elements must have same type: %s and %s", t1, t2);
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
        }
        
        Type arr;
        arr.base = SemaType::Array;
        auto *et = static_cast<Type *>(zith_arena_alloc(ctx.p->arena, sizeof(Type)));
        if (et) { *et = elem_type; arr.element_type = et; }
        arr.array_size = count;
        return arr;
    }
    case ZITH_NODE_CALL: {
        auto *call                    = static_cast<ZithCallPayload *>(expr->data.list.ptr);
        const std::string callee_name = ident_name(call ? call->callee : nullptr);
        if (call && call->callee && call->callee->type == ZITH_NODE_MEMBER) {
            sema_expr(ctx, call->callee);
            std::vector<Type> arg_types;
            for (size_t i = 0; i < call->arg_count; ++i)
                arg_types.push_back(sema_expr(ctx, call->args[i]));
            ZithNode *member_ident = call->callee->data.kids.b;
            if (member_ident && member_ident->type == ZITH_NODE_IDENTIFIER) {
                const std::string mname(member_ident->data.ident.str,
                                        member_ident->data.ident.len);
                auto it = ctx.functions.find(mname);
                if (it != ctx.functions.end()) {
                    ZithFuncPayload *match = nullptr;
                    for (auto *fn : it->second) {
                        if (fn->param_count != arg_types.size())
                            continue;
                        bool compatible = true;
                        for (size_t i = 0; i < fn->param_count; ++i) {
                            auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr);
                            if (!sema_assignable(sema_type_from_node(param->type_node, &ctx), arg_types[i])) {
                                compatible = false;
                                break;
                            }
                        }
                        if (compatible) {
                            match = fn;
                            break;
                        }
                    }
                    if (match)
                        return sema_type_from_node(match->return_type, &ctx);
                    if (!it->second.empty()) {
                        size_t expected = it->second[0]->param_count;
                        char buf[256];
                        if (arg_types.size() < expected) {
                            snprintf(buf, sizeof(buf),
                                     "too few arguments in call to '%s': expected %zu, got %zu",
                                     mname.c_str(), expected, arg_types.size());
                        } else if (arg_types.size() > expected) {
                            snprintf(buf, sizeof(buf),
                                     "too many arguments in call to '%s': expected %zu, got %zu",
                                     mname.c_str(), expected, arg_types.size());
                        } else {
                            std::string args_str;
                            for (size_t i = 0; i < arg_types.size(); ++i) {
                                char type_str[64] = {0};
                                type_to_string(arg_types[i], type_str, sizeof(type_str));
                                if (i > 0) args_str += ", ";
                                args_str += type_str;
                            }
                            snprintf(buf, sizeof(buf), "no matching overload for '%s' with (%s) argument(s)",
                                     mname.c_str(), args_str.c_str());
                        }
                        parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                    }
                }
            }
            return {};
        }
        if (callee_name == "print" || callee_name == "println") {
            for (size_t i = 0; call && i < call->arg_count; ++i)
                sema_expr(ctx, call->args[i]);
            return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        }
        std::vector<Type> arg_types;
        for (size_t i = 0; call && i < call->arg_count; ++i)
            arg_types.push_back(sema_expr(ctx, call->args[i]));
        auto it = ctx.functions.find(callee_name);
        if (it == ctx.functions.end()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "undefined function '%s'", callee_name.c_str());
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            return {};
        }
        if (ctx.analyzing_fns.count(callee_name)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "recursive function '%s' requires explicit return type annotation", callee_name.c_str());
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            return {};
        }
        const auto &overloads = it->second;
        ZithFuncPayload *match = nullptr;
        if (overloads.size() == 1) {
            ZithFuncPayload *fn = overloads[0];
            if (fn->param_count != arg_types.size()) {
                char buf[256];
                if (arg_types.size() < fn->param_count) {
                    snprintf(buf, sizeof(buf),
                             "too few arguments in call to '%s': expected %zu, got %zu",
                             callee_name.c_str(), fn->param_count, arg_types.size());
                } else {
                    snprintf(buf, sizeof(buf),
                             "too many arguments in call to '%s': expected %zu, got %zu",
                             callee_name.c_str(), fn->param_count, arg_types.size());
                }
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return {};
            }
            for (size_t i = 0; i < fn->param_count; ++i) {
                auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr);
                if (!sema_assignable(sema_type_from_node(param->type_node, &ctx), arg_types[i])) {
                    char buf[256];
                    char exp[64] = {0};
                    char got[64] = {0};
                    type_to_string(sema_type_from_node(param->type_node, &ctx), exp, sizeof(exp));
                    type_to_string(arg_types[i], got, sizeof(got));
                    snprintf(buf, sizeof(buf), "type mismatch in argument %zu to '%s': expected %s, got %s",
                             i + 1, callee_name.c_str(), exp, got);
                    parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                    return {};
                }
            }
            match = fn;
        } else {
            std::vector<ZithFuncPayload *> candidates;
            for (auto *fn : overloads) {
                if (fn->param_count != arg_types.size())
                    continue;
                bool compatible = true;
                for (size_t i = 0; i < fn->param_count; ++i) {
                    auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr);
                    if (!sema_assignable(sema_type_from_node(param->type_node, &ctx), arg_types[i])) {
                        compatible = false;
                        break;
                    }
                }
                if (compatible)
                    candidates.push_back(fn);
            }
            if (candidates.size() > 1 && expected_return.base != SemaType::Unknown) {
                std::vector<ZithFuncPayload *> filtered;
                for (auto *fn : candidates) {
                    Type ret = sema_type_from_node(fn->return_type, &ctx);
                    if (sema_assignable(expected_return, ret))
                        filtered.push_back(fn);
                }
                if (filtered.size() == 1) {
                    match = filtered[0];
                } else if (filtered.size() > 1) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "ambiguous call to overloaded '%s'", callee_name.c_str());
                    parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                    return {};
                } else {
                    char buf[512];
                    char exp_str[64] = {0};
                    type_to_string(expected_return, exp_str, sizeof(exp_str));
                    snprintf(buf, sizeof(buf), "line %zu: there's no overload of '%s' with (%zu arg%s) and expected return type %s",
                             expr->loc.line, callee_name.c_str(), arg_types.size(), arg_types.size() == 1 ? "" : "s", exp_str);
                    parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                    return {};
                }
            } else if (candidates.size() == 1) {
                match = candidates[0];
            } else if (candidates.size() > 1) {
                char buf[256];
                snprintf(buf, sizeof(buf), "ambiguous call to overloaded '%s'", callee_name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return {};
            }
            if (!match) {
                char buf[512];
                std::string args_str;
                for (size_t i = 0; i < arg_types.size(); ++i) {
                    char type_str[64] = {0};
                    type_to_string(arg_types[i], type_str, sizeof(type_str));
                    if (i > 0) args_str += ", ";
                    args_str += type_str;
                }
                snprintf(buf, sizeof(buf), "no matching overload for '%s' with (%s) argument(s)",
                         callee_name.c_str(), args_str.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return {};
            }
        }
        validate_call_ownership(ctx, call, match);
        return sema_type_from_node(match->return_type, &ctx);
    }
    case ZITH_NODE_BINARY_OP: {
        const ZithTokenType op = static_cast<ZithTokenType>(expr->data.list.len);

        // Handle assignment operators (= , +=, -=, etc.)
        if (op == ZITH_TOKEN_ASSIGNMENT || op == ZITH_TOKEN_PLUS_EQUAL ||
            op == ZITH_TOKEN_MINUS_EQUAL) {
            ZithNode *lhs = expr->data.kids.a;
            ZithNode *rhs = expr->data.kids.c;
            size_t skip_scopes = 0;
            std::string lhs_name;
            if (lhs && lhs->type == ZITH_NODE_IDENTIFIER) {
                lhs_name = ident_name(lhs);
            } else if (lhs && lhs->type == ZITH_NODE_UNARY_OP &&
                       static_cast<ZithTokenType>(lhs->data.list.len & 0xFFFF) == ZITH_TOKEN_SCOPE &&
                       lhs->data.kids.a && lhs->data.kids.a->type == ZITH_NODE_IDENTIFIER) {
                // ::x = value  — outer scope assignment
                lhs_name = ident_name(lhs->data.kids.a);
                skip_scopes = 1;
            } else {
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR,
                                 "left-hand side of assignment must be an identifier");
                return {};
            }
            bool found = false;
            auto *ve = sema_lookup_var(ctx, lhs_name, skip_scopes, &found);
            if (!found) {
                char buf[256];
                snprintf(buf, sizeof(buf), "undefined identifier '%s' in assignment", lhs_name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return {};
            }
            // Check mutability: only VAR and AUTO can be reassigned
            if (ve->binding != ZITH_BINDING_VAR && ve->binding != ZITH_BINDING_AUTO &&
                ve->binding != ZITH_BINDING_COMPTIME) {
                const char *kind = "let";
                if (ve->binding == ZITH_BINDING_CONST) kind = "const";
                char buf[256];
                snprintf(buf, sizeof(buf), "cannot reassign '%s' variable '%s'", kind, lhs_name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }

            const Type r = sema_expr(ctx, rhs, ve->type);
            if (r.base == SemaType::Void && !r.optional) {
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR,
                                 "expression has type void, cannot be used as a value");
            }
            if (ve->type.base != SemaType::Unknown && r.base != SemaType::Unknown &&
                !sema_assignable(ve->type, r)) {
                char exp[64] = {0};
                char got[64] = {0};
                type_to_string(ve->type, exp, sizeof(exp));
                type_to_string(r, got, sizeof(got));
                char buf[256];
                snprintf(buf, sizeof(buf), "type mismatch in assignment to '%s': expected %s, got %s",
                         lhs_name.c_str(), exp, got);
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
            // Clear dirty flag
            sema_set_clean(ctx, lhs_name, skip_scopes);
            return r;
        }

        // Regular arithmetic/comparison operators
        const Type l = sema_expr(ctx, expr->data.kids.a);
        const Type r = sema_expr(ctx, expr->data.kids.c);
        if (l.base == SemaType::String || r.base == SemaType::String) {
            if (op == ZITH_TOKEN_PLUS && l.base == SemaType::String &&
                r.base == SemaType::String) {
                return {SemaType::String, false, false, ZITH_OWN_DEFAULT};
            }
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR,
                             "invalid operands for binary operation");
            return {};
        }
        if (l.base == SemaType::Float || r.base == SemaType::Float)
            return {SemaType::Float, false, false, ZITH_OWN_DEFAULT};
        if (l.base == SemaType::Int && r.base == SemaType::Int) {
            switch (op) {
            case ZITH_TOKEN_EQUAL:
            case ZITH_TOKEN_NOT_EQUAL:
            case ZITH_TOKEN_LESS_THAN:
            case ZITH_TOKEN_LESS_THAN_OR_EQUAL:
            case ZITH_TOKEN_GREATER_THAN:
            case ZITH_TOKEN_GREATER_THAN_OR_EQUAL:
            case ZITH_TOKEN_AND:
            case ZITH_TOKEN_OR:
                return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT};
            default:
                return {SemaType::Int, false, false, ZITH_OWN_DEFAULT};
            }
        }
        return {};
    }
    case ZITH_NODE_STRUCT_LIT: {
        auto *p = static_cast<ZithStructLitPayload *>(expr->data.list.ptr);
        if (!p)
            return {};
        // Resolve struct type
        const Type struct_type = p->type_spec
            ? sema_type_from_node(p->type_spec, &ctx)
            : expected_return;
        if (struct_type.base != SemaType::Struct || !struct_type.struct_name) {
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR,
                             "struct literal requires a struct type");
            return {};
        }
        auto it = ctx.structs.find(struct_type.struct_name);
        if (it == ctx.structs.end()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "undefined struct type '%s'", struct_type.struct_name);
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            return {};
        }
        const auto &def = it->second;
        std::vector<bool> field_used(def.fields.size(), false);
        size_t positional_idx = 0;
        for (size_t i = 0; i < p->field_count; ++i) {
            auto *fp = p->field_inits[i];
            if (!fp || fp->type != ZITH_NODE_STRUCT_LIT_FIELD)
                continue;
            auto *ff = static_cast<ZithStructLitFieldPayload *>(fp->data.list.ptr);
            if (!ff)
                continue;

            size_t field_idx = static_cast<size_t>(-1);
            if (ff->name) {
                // Tagged field
                const std::string tag(ff->name, ff->name_len);
                for (size_t fi = 0; fi < def.fields.size(); ++fi) {
                    if (def.fields[fi].name == tag) {
                        field_idx = fi;
                        break;
                    }
                }
                if (field_idx == static_cast<size_t>(-1)) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "struct '%s' has no field '%.*s'",
                             struct_type.struct_name, static_cast<int>(ff->name_len), ff->name);
                    parser_emit_diag(ctx.p, fp->loc, ZITH_DIAG_ERROR, buf);
                    continue;
                }
            } else {
                // Untagged (positional) field — advance to next unused field
                while (positional_idx < def.fields.size() && field_used[positional_idx])
                    ++positional_idx;
                if (positional_idx >= def.fields.size()) {
                    parser_emit_diag(ctx.p, fp->loc, ZITH_DIAG_ERROR,
                                     "too many positional fields in struct literal");
                    continue;
                }
                field_idx = positional_idx;
                ++positional_idx;
            }

            if (field_used[field_idx]) {
                char buf[256];
                snprintf(buf, sizeof(buf), "duplicate field '%.*s' in struct literal",
                         ff->name ? static_cast<int>(ff->name_len) : 0, ff->name ? ff->name : "(positional)");
                parser_emit_diag(ctx.p, fp->loc, ZITH_DIAG_ERROR, buf);
                continue;
            }
            field_used[field_idx] = true;

            // Type-check the field value
            const Type field_type = sema_expr(ctx, ff->value, def.fields[field_idx].type);
            if (field_type.base != SemaType::Unknown &&
                def.fields[field_idx].type.base != SemaType::Unknown &&
                !sema_assignable(def.fields[field_idx].type, field_type)) {
                char exp[64] = {0};
                char got[64] = {0};
                type_to_string(def.fields[field_idx].type, exp, sizeof(exp));
                type_to_string(field_type, got, sizeof(got));
                char buf[256];
                snprintf(buf, sizeof(buf), "type mismatch in field '%s': expected %s, got %s",
                         def.fields[field_idx].name.c_str(), exp, got);
                parser_emit_diag(ctx.p, fp->loc, ZITH_DIAG_ERROR, buf);
            }
        }
        // Check for missing fields
        for (size_t fi = 0; fi < def.fields.size(); ++fi) {
            if (!field_used[fi]) {
                char buf[256];
                snprintf(buf, sizeof(buf), "missing field '%s' in struct literal",
                         def.fields[fi].name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
        }
        return struct_type;
    }
    case ZITH_NODE_MEMBER: {
        Type obj_type = sema_expr(ctx, expr->data.kids.a);
        if (obj_type.base == SemaType::Struct && obj_type.struct_name) {
            const std::string field_name = ident_name(expr->data.kids.b);
            auto it = ctx.structs.find(obj_type.struct_name);
            if (it != ctx.structs.end()) {
                for (const auto &sf : it->second.fields) {
                    if (sf.name == field_name)
                        return sf.type;
                }
            }
            if (!field_name.empty()) {
                char buf[256];
                snprintf(buf, sizeof(buf), "struct '%s' has no field '%s'",
                         obj_type.struct_name, field_name.c_str());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
        }
        return obj_type;
    }
    case ZITH_NODE_UNARY_OP: {
        const ZithTokenType op = static_cast<ZithTokenType>(expr->data.list.len & 0xFFFF);
        if (op == ZITH_TOKEN_SCOPE && expr->data.kids.a &&
            expr->data.kids.a->type == ZITH_NODE_IDENTIFIER) {
            const std::string name = ident_name(expr->data.kids.a);
            return sema_lookup(ctx, name, 1);
        }
        return sema_expr(ctx, expr->data.kids.a);
    }
    case ZITH_NODE_CALL_ARG: {
        auto *p = static_cast<ZithCallArgPayload *>(expr->data.list.ptr);
        if (!p)
            return {};
        Type t = sema_expr(ctx, p->expr);
        if (p->ownership != ZITH_OWN_DEFAULT)
            t.ownership = p->ownership;
        return t;
    }
    case ZITH_NODE_CAST: {
        Type dst = sema_type_from_node(expr->data.kids.b, &ctx);
        if (dst.base == SemaType::Unknown) {
            char buf[256];
            const char *type_str = expr->data.kids.b->data.ident.str;
            size_t type_len = expr->data.kids.b->data.ident.len;
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", static_cast<int>(type_len), type_str);
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            return dst;
        }
        Type src = sema_expr(ctx, expr->data.kids.a, dst);
        if (src.base != SemaType::Unknown && src.base != dst.base) {
            bool src_numeric = (src.base == SemaType::Int || src.base == SemaType::Float);
            bool dst_numeric = (dst.base == SemaType::Int || dst.base == SemaType::Float);
            if (!src_numeric || !dst_numeric) {
                char buf[256];
                char src_str[64] = {0};
                char dst_str[64] = {0};
                type_to_string(src, src_str, sizeof(src_str));
                type_to_string(dst, dst_str, sizeof(dst_str));
                snprintf(buf, sizeof(buf), "invalid cast from %s to %s", src_str, dst_str);
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
            }
        }
        return dst;
    }
    default:
        return {};
    }
}

static void validate_call_ownership(SemaContext &ctx, ZithCallPayload *call, ZithFuncPayload *fn) {
    if (!call || !fn)
        return;
    size_t count = fn->param_count < call->arg_count ? fn->param_count : call->arg_count;
    for (size_t i = 0; i < count; ++i) {
        auto *param = static_cast<ZithParamPayload *>(fn->params[i]->data.list.ptr);
        ZithOwnership param_own = param->ownership;

        // Get call-site annotation
        ZithNode *arg = call->args[i];
        ZithOwnership arg_own = ZITH_OWN_DEFAULT;
        if (arg->type == ZITH_NODE_CALL_ARG) {
            auto *ap = static_cast<ZithCallArgPayload *>(arg->data.list.ptr);
            arg_own = ap->ownership;
        }

        // Get source ownership from the expression's resolved type
        Type src_type = sema_expr(ctx, arg);
        ZithOwnership src_own = src_type.ownership;

        // RULE 1: view/lend/extension params MUST be annotated
        if (param_own == ZITH_OWN_VIEW && arg_own != ZITH_OWN_VIEW) {
            char buf[256];
            snprintf(buf, sizeof(buf), "argument %zu to '%s' must be annotated with 'view'", i + 1, fn->name);
            parser_emit_diag(ctx.p, call->args[i]->loc, ZITH_DIAG_ERROR, buf);
        }
        if (param_own == ZITH_OWN_LEND && arg_own != ZITH_OWN_LEND) {
            char buf[256];
            snprintf(buf, sizeof(buf), "argument %zu to '%s' must be annotated with 'lend'", i + 1, fn->name);
            parser_emit_diag(ctx.p, call->args[i]->loc, ZITH_DIAG_ERROR, buf);
        }
        if (param_own == ZITH_OWN_EXTENSION && arg_own != ZITH_OWN_EXTENSION) {
            char buf[256];
            snprintf(buf, sizeof(buf), "argument %zu to '%s' must be annotated with 'extension'", i + 1, fn->name);
            parser_emit_diag(ctx.p, call->args[i]->loc, ZITH_DIAG_ERROR, buf);
        }

        // RULE 2: shared params MUST be annotated unless source is already shared
        if (param_own == ZITH_OWN_SHARED && arg_own != ZITH_OWN_SHARED && src_own != ZITH_OWN_SHARED) {
            char buf[256];
            snprintf(buf, sizeof(buf), "argument %zu to '%s' must be annotated with 'share' (source is not shared)", i + 1, fn->name);
            parser_emit_diag(ctx.p, call->args[i]->loc, ZITH_DIAG_ERROR, buf);
        }

        // RULE 3: Redundant annotation warning (share on share)
        if (arg_own == ZITH_OWN_SHARED && src_own == ZITH_OWN_SHARED) {
            char buf[256];
            snprintf(buf, sizeof(buf), "redundant 'share' annotation — source is already shared");
            parser_emit_diag(ctx.p, call->args[i]->loc, ZITH_DIAG_WARNING, buf);
        }

        // RULE 4: Literals → copied for default/unique params, no view needed
        // (Handled naturally: literals have ZITH_OWN_DEFAULT, and the rules above
        //  only enforce view/lend/extension/shared when the param requires it)
    }
}

} // namespace

static bool sema_validate_import(Parser *p, const char *path, size_t path_len, ZithSourceLoc loc) {
    if (!p->import_roots || p->import_root_count == 0)
        return true;

    std::string import_root;
    for (size_t i = 0; i < path_len; ++i) {
        const char c = path[i];
        if (c == '.' || c == '/')
            break;
        import_root.push_back(c);
    }

    if (import_root.empty())
        return true;

    bool allowed = false;
    for (size_t i = 0; i < p->import_root_count; ++i) {
        const char *root_full = p->import_roots[i];
        const char *slash     = strrchr(root_full, '/');
        const char *root_name = slash ? slash + 1 : root_full;
        if (import_root == root_name) {
            allowed = true;
            break;
        }
    }
    if (allowed)
        return true;

    char buf[256];
    snprintf(buf, sizeof(buf), "import '%s' is not from an allowed directory",
             import_root.c_str());
    parser_emit_diag(p, loc, ZITH_DIAG_ERROR, buf);
    return false;
}

void sema_clear_import_state() {
    zith::import::ModuleRegistry::instance().clear();
}

void sema_run(Parser *p, ZithNode *root) {
    if (!root || root->type != ZITH_NODE_PROGRAM)
        return;
    SemaContext ctx{};
    ctx.p = p;

    extern void *parser_get_imported_decls(void);
    void *imported_decls_ptr = parser_get_imported_decls();
    if (imported_decls_ptr) {
        auto *imported_vec = static_cast<std::vector<ZithNode *> *>(imported_decls_ptr);
        for (auto *decl : *imported_vec) {
            if (decl && decl->type == ZITH_NODE_FUNC_DECL) {
                auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
                if (fn && fn->name) {
                    ctx.functions[std::string(fn->name, fn->name_len)].push_back(fn);
                }
            }
        }
    }

    auto find_imported_fn = [&](const std::string &name) -> ZithFuncPayload * {
        if (!imported_decls_ptr) return nullptr;
        auto *imported_vec = static_cast<std::vector<ZithNode *> *>(imported_decls_ptr);
        for (auto *decl : *imported_vec) {
            if (decl && decl->type == ZITH_NODE_FUNC_DECL) {
                auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
                if (fn && fn->name && std::string(fn->name, fn->name_len) == name)
                    return fn;
            }
        }
        return nullptr;
    };

    auto find_imported_struct = [&](const std::string &name) -> ZithStructPayload * {
        if (!imported_decls_ptr) return nullptr;
        auto *imported_vec = static_cast<std::vector<ZithNode *> *>(imported_decls_ptr);
        for (auto *decl : *imported_vec) {
            if (decl && decl->type == ZITH_NODE_STRUCT_DECL) {
                auto *sd = static_cast<ZithStructPayload *>(decl->data.list.ptr);
                if (sd && sd->name && std::string(sd->name, sd->name_len) == name)
                    return sd;
            }
        }
        return nullptr;
    };

    auto inject_module_symbols = [&](const std::string &module_path, ZithSourceLoc loc) {
        auto mod = zith::import::ModuleRegistry::instance().get_module(module_path);
        if (!mod) {
            char buf[256];
            snprintf(buf, sizeof(buf), "module '%s' not found", module_path.c_str());
            parser_emit_diag(p, loc, ZITH_DIAG_ERROR, buf);
            return;
        }
        for (const auto &sym : mod->symbols()) {
            if (!sym.is_exported()) continue;
            if (sym.kind() == zith::import::SymbolKind::Function) {
                if (ctx.functions.count(sym.name())) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "conflict: function '%s' already defined", sym.name().c_str());
                    parser_emit_diag(p, loc, ZITH_DIAG_ERROR, buf);
                    continue;
                }
                ZithFuncPayload *fn = find_imported_fn(sym.name());
                if (fn) {
                    ctx.functions[sym.name()].push_back(fn);
                }
            } else if (sym.kind() == zith::import::SymbolKind::Struct) {
                if (ctx.structs.count(sym.name())) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "conflict: struct '%s' already defined", sym.name().c_str());
                    parser_emit_diag(p, loc, ZITH_DIAG_ERROR, buf);
                    continue;
                }
                ZithStructPayload *sd = find_imported_struct(sym.name());
                if (sd) {
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
                }
            }
        }
    };

    for (const auto &imp : get_tls_parse_ctx().from_imports) {
        inject_module_symbols(imp.module_path, imp.loc);
    }

    for (const auto &re : get_tls_parse_ctx().re_exports) {
        inject_module_symbols(re.module_path, re.loc);
    }

    auto **decls = static_cast<ZithNode **>(root->data.list.ptr);
    for (size_t i = 0; i < root->data.list.len; ++i) {
        ZithNode *decl = decls[i];
        if (!decl)
            continue;
        if (decl->type == ZITH_NODE_FUNC_DECL) {
            auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
            auto &overloads = ctx.functions[std::string(fn->name, fn->name_len)];
            for (auto *existing : overloads) {
                if (existing->param_count != fn->param_count)
                    continue;
                bool same = true;
                for (size_t pi = 0; pi < fn->param_count; ++pi) {
                    auto *ep = static_cast<ZithParamPayload *>(existing->params[pi]->data.list.ptr);
                    auto *fp = static_cast<ZithParamPayload *>(fn->params[pi]->data.list.ptr);
if (!zith::type_match(sema_type_from_node(ep->type_node, &ctx),
                                          sema_type_from_node(fp->type_node, &ctx))) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    Type existing_ret = sema_type_from_node(existing->return_type, &ctx);
                    Type new_ret = sema_type_from_node(fn->return_type, &ctx);
                    if (zith::type_match(existing_ret, new_ret)) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "duplicate definition of '%s' with same parameter types",
                                 fn->name);
                        parser_emit_diag(p, decl->loc, ZITH_DIAG_ERROR, buf);
                        break;
                    }
                }
            }
            overloads.push_back(fn);
            continue;
        }
        if (decl->type == ZITH_NODE_IMPORT) {
            auto *imp = static_cast<ZithImportPayload *>(decl->data.list.ptr);
            if (!imp || !imp->path)
                continue;

            if (!sema_validate_import(p, imp->path, imp->path_len, decl->loc))
                continue;

            std::string root_name;
            for (size_t j = 0; j < imp->path_len; ++j) {
                const char c = imp->path[j];
                if (c == '.' || c == '/')
                    break;
                root_name.push_back(c);
            }
            if (!root_name.empty())
                ctx.imported_roots.insert(root_name);

            // Register module in import system
            const std::string path_str(imp->path, imp->path_len);
            if (!root_name.empty() && !zith::import::ModuleRegistry::instance().exists(root_name)) {
                zith::import::Module mod(root_name, "");
                zith::import::ModuleRegistry::instance().register_module(std::move(mod));
            }

            if (imp->alias && imp->alias_len > 0) {
                const std::string alias_str(imp->alias, imp->alias_len);
                sema_define(ctx, alias_str,
                            {SemaType::Module, false, false, ZITH_OWN_DEFAULT});
            } else if (!root_name.empty()) {
                sema_define(ctx, root_name, {SemaType::Module, false, false, ZITH_OWN_DEFAULT});
            }
        }
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
    }

    for (size_t i = 0; i < root->data.list.len; ++i) {
        ZithNode *decl = decls[i];
        if (!decl || decl->type != ZITH_NODE_FUNC_DECL)
            continue;
        auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
        if (fn->return_type)
            continue;

        std::string fn_name(fn->name, fn->name_len);
        ctx.analyzing_fns.insert(fn_name);

        SemaContext infer_ctx{};
        infer_ctx.p = p;
        infer_ctx.functions = ctx.functions;
        infer_ctx.structs = ctx.structs;
        infer_ctx.analyzing_fns = ctx.analyzing_fns;

        sema_push_scope(infer_ctx);
        for (size_t pi = 0; pi < fn->param_count; ++pi) {
            auto *param = static_cast<ZithParamPayload *>(fn->params[pi]->data.list.ptr);
            Type param_type = sema_type_from_node(param->type_node, &infer_ctx);
            sema_define(infer_ctx, std::string(param->name, param->name_len), param_type);
        }
        Type inferred = sema_stmt(infer_ctx, fn->body);
        sema_pop_scope(infer_ctx);

        ctx.analyzing_fns.erase(fn_name);
        ctx.inferred_returns[fn] = inferred;
    }

    for (size_t i = 0; i < root->data.list.len; ++i) {
        ZithNode *decl = decls[i];
        if (!decl || decl->type != ZITH_NODE_FUNC_DECL)
            continue;
        auto *fn = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
        sema_push_scope(ctx);
        if (fn->return_type) {
            ctx.current_return = sema_type_from_node(fn->return_type, &ctx);
            if (ctx.current_return.base == SemaType::Unknown) {
                char buf[256];
                const char *type_str = fn->return_type->data.ident.str;
                size_t type_len = fn->return_type->data.ident.len;
                snprintf(buf, sizeof(buf), "undefined type '%.*s'", static_cast<int>(type_len), type_str);
                parser_emit_diag(ctx.p, decl->loc, ZITH_DIAG_ERROR, buf);
                ctx.current_return = {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
            }
        } else if (ctx.inferred_returns.count(fn)) {
            ctx.current_return = ctx.inferred_returns[fn];
        } else {
            ctx.current_return = {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        }
        for (size_t pi = 0; pi < fn->param_count; ++pi) {
            auto *param = static_cast<ZithParamPayload *>(fn->params[pi]->data.list.ptr);
            Type param_type = sema_type_from_node(param->type_node, &ctx);
            if (param->type_node && param_type.base == SemaType::Unknown) {
                char buf[256];
                const char *type_str = param->type_node->data.ident.str;
                size_t type_len = param->type_node->data.ident.len;
                snprintf(buf, sizeof(buf), "undefined type '%.*s'", static_cast<int>(type_len), type_str);
                parser_emit_diag(ctx.p, fn->params[pi]->loc, ZITH_DIAG_ERROR, buf);
            }
            sema_define(ctx, std::string(param->name, param->name_len), param_type);
        }
        sema_stmt(ctx, fn->body);
        sema_pop_scope(ctx);
    }

    // Clear import system state after each parse
    zith::import::ModuleRegistry::instance().clear();
}
