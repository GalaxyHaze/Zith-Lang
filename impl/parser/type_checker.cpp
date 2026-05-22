#include "type_checker.hpp"
#include "diagnostics/diagnostics.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace zith {

TypeChecker::TypeChecker(TypeContext& ctx, DiagManager* diag_mgr, ZithArena* arena)
    : ctx_(ctx), diag_(diag_mgr), arena_(arena) {}

bool TypeChecker::is_exact_match(const Type& lhs, const Type& rhs) {
    return zith::type_match(lhs, rhs);
}

bool TypeChecker::is_coercible(const Type& dst, const Type& src) {
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

bool TypeChecker::is_assignable(const Type& dst, const Type& src) {
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

Type TypeChecker::type_from_node(const ZithNode* n) {
    if (!n)
        return {};
    if (n->type == ZITH_NODE_UNARY_OP) {
        Type inner = type_from_node(n->data.kids.a);
        const ZithTokenType op = static_cast<ZithTokenType>(n->data.list.len & 0xFFFF);
        if (op == ZITH_TOKEN_QUESTION)
            inner.optional = true;
        else if (op == ZITH_TOKEN_BANG)
            inner.failable = true;
        return inner;
    }
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
            Type inner = type_from_node(n->data.kids.a);
            inner.ownership = own;
            return inner;
        }
    }
    if (n->type == ZITH_NODE_TYPE_POINTER) {
        return {SemaType::VoidPtr, false, false, ZITH_OWN_DEFAULT};
    }
    if (n->type == ZITH_NODE_TYPE_ARRAY) {
        ZithNode* elem = static_cast<ZithNode*>(n->data.list.ptr);
        if (!elem)
            return {};
        Type inner = type_from_node(elem);
        if (inner.base != SemaType::Unknown) {
            Type arr;
            arr.base = SemaType::Array;
            if (arena_) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena_, sizeof(Type)));
                if (et) { *et = inner; arr.element_type = et; }
            }
            arr.array_size = n->data.list.len;
            return arr;
        }
        return {};
    }
    if (n->type == ZITH_NODE_TYPE_SLICE) {
        ZithNode* elem = static_cast<ZithNode*>(n->data.list.ptr);
        if (!elem)
            return {};
        Type inner = type_from_node(elem);
        if (inner.base != SemaType::Unknown) {
            Type slice;
            slice.base = SemaType::Slice;
            if (arena_) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena_, sizeof(Type)));
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
        if (const auto it = ctx_.structs.find(name); it != ctx_.structs.end())
            return {SemaType::Struct, false, false, ZITH_OWN_DEFAULT, it->second.name.c_str(), nullptr};
        return {};
    }
    return {};
}

void TypeChecker::emit_error(ZithSourceLoc loc, zith::diag::DiagCode code, const char* msg) {
    if (!diag_)
        return;
    using namespace zith::diag;
    const SourceSpan span = SourceSpan::from_loc(loc, diag_->source_map().add_or_get_file("<input>", ""));
    DiagnosticBuilder(DiagLevel::Error, code, span)
        .with_raw_message(msg ? msg : "")
        .emit(diag_->bag());
}

Type TypeChecker::get_type(ZithNode* node, Type expected_return) {
    if (!node)
        return {SemaType::Void, false, false, ZITH_OWN_DEFAULT};
    switch (node->type) {
    case ZITH_NODE_LITERAL: {
        auto* lit = static_cast<ZithLiteral*>(node->data.list.ptr);
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
            if (arena_) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena_, sizeof(Type)));
                if (et) { *et = char_type; str.element_type = et; }
            }
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
    case ZITH_NODE_BINARY_OP: {
        const ZithTokenType op = static_cast<ZithTokenType>(node->data.list.len);
        if (op == ZITH_TOKEN_ASSIGNMENT || op == ZITH_TOKEN_PLUS_EQUAL ||
            op == ZITH_TOKEN_MINUS_EQUAL) {
            ZithNode* lhs = node->data.kids.a;
            ZithNode* rhs = node->data.kids.c;
            if (!lhs || !rhs)
                return {};
            return get_type(rhs, expected_return);
        }
        const Type l = get_type(node->data.kids.a);
        const Type r = get_type(node->data.kids.c);
        if (l.base == SemaType::String || r.base == SemaType::String) {
            if (op == ZITH_TOKEN_PLUS && l.base == SemaType::String &&
                r.base == SemaType::String) {
                return {SemaType::String, false, false, ZITH_OWN_DEFAULT};
            }
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
    case ZITH_NODE_CALL_ARG: {
        auto* p = static_cast<ZithCallArgPayload*>(node->data.list.ptr);
        if (!p)
            return {};
        return get_type(p->expr);
    }
    case ZITH_NODE_UNARY_OP: {
        const ZithTokenType op = static_cast<ZithTokenType>(node->data.list.len & 0xFFFF);
        if (op == ZITH_TOKEN_SCOPE && node->data.kids.a &&
            node->data.kids.a->type == ZITH_NODE_IDENTIFIER) {
            return get_type(node->data.kids.a);
        }
        return get_type(node->data.kids.a);
    }
    case ZITH_NODE_CAST: {
        Type dst = type_from_node(node->data.kids.b);
        return dst;
    }
    case ZITH_NODE_MEMBER: {
        Type obj_type = get_type(node->data.kids.a);
        if (obj_type.base == SemaType::Struct && obj_type.struct_name) {
            const StructDef* def = ctx_.get_struct_def(obj_type.struct_name);
            if (def) {
                std::string field_name;
                if (node->data.kids.b && node->data.kids.b->type == ZITH_NODE_IDENTIFIER) {
                    field_name = std::string(node->data.kids.b->data.ident.str,
                                             node->data.kids.b->data.ident.len);
                }
                for (const auto& sf : def->fields) {
                    if (sf.name == field_name)
                        return sf.type;
                }
            }
        }
        return obj_type;
    }
    case ZITH_NODE_STRUCT_LIT: {
        auto* p = static_cast<ZithStructLitPayload*>(node->data.list.ptr);
        if (!p)
            return {};
        const Type struct_type = p->type_spec
            ? type_from_node(p->type_spec)
            : expected_return;
        if (struct_type.base != SemaType::Struct || !struct_type.struct_name) {
            return {};
        }
        check_struct_literal(node, struct_type);
        return struct_type;
    }
    case ZITH_NODE_ARRAY_LIT: {
        auto** items = static_cast<ZithNode**>(node->data.list.ptr);
        size_t count = node->data.list.len;
        if (count == 0) {
            if (expected_return.base == SemaType::Array || expected_return.base == SemaType::Slice) {
                Type arr = expected_return;
                arr.array_size = count;
                return arr;
            }
            Type arr;
            arr.base = SemaType::Array;
            if (arena_) {
                auto *et = static_cast<Type *>(zith_arena_alloc(arena_, sizeof(Type)));
                if (et) { *et = Type{SemaType::Int, false, false, ZITH_OWN_DEFAULT}; arr.element_type = et; }
            }
            arr.array_size = 0;
            return arr;
        }
        Type elem_type = get_type(items[0]);
        Type arr;
        arr.base = SemaType::Array;
        if (arena_) {
            auto *et = static_cast<Type *>(zith_arena_alloc(arena_, sizeof(Type)));
            if (et) { *et = elem_type; arr.element_type = et; }
        }
        arr.array_size = count;
        return arr;
    }
    default:
        return {};
    }
}

void TypeChecker::check_struct_literal(ZithNode* expr, const Type& struct_type) {
    auto* p = static_cast<ZithStructLitPayload*>(expr->data.list.ptr);
    if (!p || !struct_type.struct_name)
        return;
    const StructDef* def = ctx_.get_struct_def(struct_type.struct_name);
    if (!def)
        return;
    std::vector<bool> field_used(def->fields.size(), false);
    size_t positional_idx = 0;
    for (size_t i = 0; i < p->field_count; ++i) {
        auto* fp = p->field_inits[i];
        if (!fp || fp->type != ZITH_NODE_STRUCT_LIT_FIELD)
            continue;
        auto* ff = static_cast<ZithStructLitFieldPayload*>(fp->data.list.ptr);
        if (!ff)
            continue;
        size_t field_idx = (size_t)-1;
        if (ff->name) {
            const std::string tag(ff->name, ff->name_len);
            for (size_t fi = 0; fi < def->fields.size(); ++fi) {
                if (def->fields[fi].name == tag) {
                    field_idx = fi;
                    break;
                }
            }
            if (field_idx == (size_t)-1) {
                char buf[256];
                snprintf(buf, sizeof(buf), "struct '%s' has no field '%.*s'",
                         struct_type.struct_name, (int)ff->name_len, ff->name);
                emit_error(fp->loc, zith::diag::DiagCode::UnexpectedToken, buf);
                continue;
            }
        } else {
            while (positional_idx < def->fields.size() && field_used[positional_idx])
                ++positional_idx;
            if (positional_idx >= def->fields.size()) {
                emit_error(fp->loc, zith::diag::DiagCode::UnexpectedToken,
                           "too many positional fields in struct literal");
                continue;
            }
            field_idx = positional_idx;
            ++positional_idx;
        }
        if (field_used[field_idx]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "duplicate field '%.*s' in struct literal",
                     ff->name ? (int)ff->name_len : 0, ff->name ? ff->name : "(positional)");
            emit_error(fp->loc, zith::diag::DiagCode::UnexpectedToken, buf);
            continue;
        }
        field_used[field_idx] = true;
        const Type field_type = get_type(ff->value, def->fields[field_idx].type);
        if (field_type.base != SemaType::Unknown &&
            def->fields[field_idx].type.base != SemaType::Unknown &&
            !is_assignable(def->fields[field_idx].type, field_type)) {
            char buf[256];
            char exp[64] = {0};
            char got[64] = {0};
            snprintf(exp, sizeof(exp), "%s", base_type_name(def->fields[field_idx].type.base));
            snprintf(got, sizeof(got), "%s", base_type_name(field_type.base));
            snprintf(buf, sizeof(buf), "type mismatch in field '%s': expected %s, got %s",
                     def->fields[field_idx].name.c_str(), exp, got);
            emit_error(fp->loc, zith::diag::DiagCode::TypeMismatch, buf);
        }
    }
    for (size_t fi = 0; fi < def->fields.size(); ++fi) {
        if (!field_used[fi]) {
            char buf[256];
            snprintf(buf, sizeof(buf), "missing field '%s' in struct literal",
                     def->fields[fi].name.c_str());
            emit_error(expr->loc, zith::diag::DiagCode::UnexpectedToken, buf);
        }
    }
}

void TypeChecker::check_assignment(const std::string& lhs_name, const Type& lhs_type,
                                   ZithNode* /*rhs_expr*/, Type rhs_type, ZithSourceLoc loc) {
    if (rhs_type.base == SemaType::Void && !rhs_type.optional) {
        emit_error(loc, zith::diag::DiagCode::UnexpectedToken,
                   "expression has type void, cannot be used as a value");
    }
    if (lhs_type.base != SemaType::Unknown && rhs_type.base != SemaType::Unknown &&
        !is_assignable(lhs_type, rhs_type)) {
        char buf[256];
        char exp[64] = {0};
        char got[64] = {0};
        snprintf(exp, sizeof(exp), "%s", base_type_name(lhs_type.base));
        snprintf(got, sizeof(got), "%s", base_type_name(rhs_type.base));
        snprintf(buf, sizeof(buf), "type mismatch in assignment to '%s': expected %s, got %s",
                 lhs_name.c_str(), exp, got);
        emit_error(loc, zith::diag::DiagCode::TypeMismatch, buf);
    }
}

} // namespace zith
