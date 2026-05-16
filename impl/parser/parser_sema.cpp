// impl/parser/parser_sema.cpp — Semantic Analysis Phase
//
// Refactored from parser.cpp. Handles type checking, name resolution,
// scope management, and control-flow analysis.
#include "memory/arena.hpp"
#include "zith/parser.h"
#include "zith/typesystem.hpp"
#include "import/import.hpp"
#include "import/module_registry.hpp"
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

struct SemaScope {
    std::unordered_map<std::string, Type> vars;
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
    case SemaType::Int:
        return "int";
    case SemaType::Float:
        return "float";
    case SemaType::String:
        return "string";
    case SemaType::Bool:
        return "bool";
    case SemaType::Module:
        return "module";
    case SemaType::Struct:
        return "struct";
    default:
        return "unknown";
    }
}

static void type_to_string(const Type &t, char *out, size_t out_len) {
    if (!out || out_len == 0)
        return;
    const char *base = base_type_name(t.base);
    snprintf(out, out_len, "%s%s%s", base, t.optional ? "?" : "", t.failable ? "!" : "");
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
    if (n->type == ZITH_NODE_IDENTIFIER && n->data.ident.str) {
        const std::string name(n->data.ident.str, n->data.ident.len);
        if (name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "u8" ||
            name == "u16" || name == "u32" || name == "u64" || name == "int")
            return {SemaType::Int, false, false, ZITH_OWN_DEFAULT};
        if (name == "f32" || name == "f64" || name == "float")
            return {SemaType::Float, false, false, ZITH_OWN_DEFAULT};
        if (name == "str" || name == "string")
            return {SemaType::String, false, false, ZITH_OWN_DEFAULT};
        if (name == "bool")
            return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT};
        if (name == "void")
            return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        if (ctx) {
            auto it = ctx->structs.find(name);
            if (it != ctx->structs.end())
                return {SemaType::Struct, false, false, ZITH_OWN_DEFAULT, it->second.name.c_str()};
        }
        return {};
    }
    return {};
}

static bool sema_assignable(const Type &dst, const Type &src) {
    if (dst.base == SemaType::Unknown || src.base == SemaType::Unknown)
        return true;
    if (dst.base != src.base)
        return false;
    if (dst.base == SemaType::Struct) {
        if (!dst.struct_name || !src.struct_name)
            return false;
        if (strcmp(dst.struct_name, src.struct_name) != 0)
            return false;
    }
    if (!dst.optional && src.optional)
        return false;
    if (!dst.failable && src.failable)
        return false;
    return true;
}

static void sema_push_scope(SemaContext &ctx) {
    ctx.scopes.push_back({});
}
static void sema_pop_scope(SemaContext &ctx) {
    if (!ctx.scopes.empty())
        ctx.scopes.pop_back();
}

static void sema_define(SemaContext &ctx, const std::string &name, Type t) {
    if (ctx.scopes.empty())
        sema_push_scope(ctx);
    ctx.scopes.back().vars[name] = t;
}

static Type sema_lookup(const SemaContext &ctx, const std::string &name, size_t skip_scopes = 0,
                        bool *found_out = nullptr) {
    if (skip_scopes >= ctx.scopes.size()) {
        if (found_out) *found_out = false;
        return {};
    }
    for (auto it = ctx.scopes.rbegin() + skip_scopes; it != ctx.scopes.rend(); ++it) {
        auto found = it->vars.find(name);
        if (found != it->vars.end()) {
            if (found_out) *found_out = true;
            return found->second;
        }
    }
    if (found_out) *found_out = false;
    return {};
}

static Type sema_expr(SemaContext &ctx, ZithNode *expr);
static void validate_call_ownership(SemaContext &ctx, ZithCallPayload *call, ZithFuncPayload *fn);

static void sema_stmt(SemaContext &ctx, ZithNode *stmt) {
    if (!stmt)
        return;
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
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", (int)type_len, type_str);
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
        }
        Type init_type = sema_expr(ctx, p->initializer);
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
            sema_define(ctx, name, var_type);
        }
        break;
    }
    case ZITH_NODE_VAR_DECL: {
        auto *var = static_cast<ZithVarPayload *>(stmt->data.list.ptr);
        const std::string name(var->name, var->name_len);
        const Type declared = sema_type_from_node(var->type_node, &ctx);
        if (var->type_node && declared.base == SemaType::Unknown) {
            char buf[256];
            const char *type_str = var->type_node->data.ident.str;
            size_t type_len = var->type_node->data.ident.len;
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", (int)type_len, type_str);
            parser_emit_diag(ctx.p, stmt->loc, ZITH_DIAG_ERROR, buf);
        }
        const Type init     = sema_expr(ctx, var->initializer);
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
        sema_define(ctx, name, declared.base != SemaType::Unknown ? declared : init);
        break;
    }
    case ZITH_NODE_RETURN: {
        const Type ret = sema_expr(ctx, stmt->data.kids.a);
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
        break;
    }
    case ZITH_NODE_IF:
        sema_expr(ctx, stmt->data.kids.a);
        sema_stmt(ctx, stmt->data.kids.b);
        sema_stmt(ctx, stmt->data.kids.c);
        break;
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
        break;
    }
    case ZITH_NODE_BLOCK: {
        sema_push_scope(ctx);
        auto **stmts = static_cast<ZithNode **>(stmt->data.list.ptr);
        for (size_t i = 0; i < stmt->data.list.len; ++i)
            sema_stmt(ctx, stmts[i]);
        sema_pop_scope(ctx);
        break;
    }
    default:
        sema_expr(ctx, stmt);
        break;
    }
}

static Type sema_expr(SemaContext &ctx, ZithNode *expr) {
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
        case ZITH_LIT_STRING:
            return {SemaType::String, false, false, ZITH_OWN_DEFAULT};
        case ZITH_LIT_BOOL:
            return {SemaType::Bool, false, false, ZITH_OWN_DEFAULT};
        }
        return {};
    }
    case ZITH_NODE_IDENTIFIER: {
        const std::string name = ident_name(expr);
        bool found = false;
        const Type t = sema_lookup(ctx, name, 0, &found);
        if (!found && !name.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "undefined identifier '%s'", name.c_str());
            parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
        }
        return t;
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
                            snprintf(buf, sizeof(buf),
                                     "no matching overload for '%s' with given argument types",
                                     mname.c_str());
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
                if (compatible) {
                    if (match) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "ambiguous call to overloaded '%s'", callee_name.c_str());
                        parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                        return {};
                    }
                    match = fn;
                }
            }
            if (!match) {
                char buf[256];
                snprintf(buf, sizeof(buf), "no matching overload for '%s' with %zu argument(s)",
                         callee_name.c_str(), arg_types.size());
                parser_emit_diag(ctx.p, expr->loc, ZITH_DIAG_ERROR, buf);
                return {};
            }
        }
        validate_call_ownership(ctx, call, match);
        return sema_type_from_node(match->return_type, &ctx);
    }
    case ZITH_NODE_BINARY_OP: {
        const Type l = sema_expr(ctx, expr->data.kids.a);
        const Type r = sema_expr(ctx, expr->data.kids.c);
        if (l.base == SemaType::String || r.base == SemaType::String) {
            if (expr->data.list.len == ZITH_TOKEN_PLUS && l.base == SemaType::String &&
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
            switch (static_cast<ZithTokenType>(expr->data.list.len)) {
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
        return sema_expr(ctx, p->expr);
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
                    char buf[256];
                    snprintf(buf, sizeof(buf), "duplicate definition of '%s' with same parameter types",
                             fn->name);
                    parser_emit_diag(p, decl->loc, ZITH_DIAG_ERROR, buf);
                    break;
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
        sema_push_scope(ctx);
        ctx.current_return = sema_type_from_node(fn->return_type, &ctx);
        if (fn->return_type && ctx.current_return.base == SemaType::Unknown) {
            char buf[256];
            const char *type_str = fn->return_type->data.ident.str;
            size_t type_len = fn->return_type->data.ident.len;
            snprintf(buf, sizeof(buf), "undefined type '%.*s'", (int)type_len, type_str);
            parser_emit_diag(ctx.p, decl->loc, ZITH_DIAG_ERROR, buf);
            ctx.current_return = {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        }
        if (ctx.current_return.base == SemaType::Unknown)
            ctx.current_return = {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
        for (size_t pi = 0; pi < fn->param_count; ++pi) {
            auto *param = static_cast<ZithParamPayload *>(fn->params[pi]->data.list.ptr);
            Type param_type = sema_type_from_node(param->type_node, &ctx);
            if (param->type_node && param_type.base == SemaType::Unknown) {
                char buf[256];
                const char *type_str = param->type_node->data.ident.str;
                size_t type_len = param->type_node->data.ident.len;
                snprintf(buf, sizeof(buf), "undefined type '%.*s'", (int)type_len, type_str);
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