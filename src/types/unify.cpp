#include "unify.hpp"
#include "diagnostics/error-codes.hpp"

#include <span>
#include <variant>

namespace zith::types {

Unifier::Unifier(TypeIntern &intern, diagnostics::DiagnosticEngine &diags, memory::Arena &arena)
    : intern_(intern), diags_(diags), subst_(arena) {}

TypeId Unifier::freshVar() {
    return intern_.internTypeVar();
}

TypeId Unifier::substitute(TypeId t) const {
    for (;;) {
        if (t >= subst_.size())
            return t;
        TypeId next = subst_[t];
        if (next == t || next == kInvalidType)
            return t;
        t = next;
    }
}

bool Unifier::occurs(TypeId var, TypeId t) const {
    TypeId resolved = substitute(t);
    if (resolved == var)
        return true;
    if (resolved >= intern_.count())
        return false;

    auto kind = intern_.kindOf(resolved);
    auto &data = intern_.lookup(resolved);

    switch (kind) {
    case TypeKind::Ptr:
        return occurs(var, std::get<TypePtr>(data).pointee);
    case TypeKind::Array:
        return occurs(var, std::get<TypeArray>(data).elem);
    case TypeKind::Optional:
        return occurs(var, std::get<TypeOptional>(data).inner);
    case TypeKind::Failable:
        return occurs(var, std::get<TypeFailable>(data).inner);
    case TypeKind::Fn: {
        auto &fn = std::get<TypeFn>(data);
        if (occurs(var, fn.ret))
            return true;
        for (auto p : fn.params) {
            if (occurs(var, p))
                return true;
        }
        return false;
    }
    default:
        return false;
    }
}

bool Unifier::unify(TypeId a, TypeId b, memory::Span span) {
    a = substitute(a);
    b = substitute(b);

    if (a == b)
        return true;

    auto kind_a = intern_.kindOf(a);
    auto kind_b = intern_.kindOf(b);

    if (kind_a == TypeKind::TypeVar) {
        if (occurs(a, b)) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::CyclicType,
                          "cyclic type: type variable contains itself", span);
            return false;
        }
        if (a >= subst_.size())
            subst_.reserve(a + 1);
        while (subst_.size() <= a)
            subst_.push(kInvalidType);
        subst_[a] = b;
        return true;
    }

    if (kind_b == TypeKind::TypeVar) {
        if (occurs(b, a)) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::CyclicType,
                          "cyclic type: type variable contains itself", span);
            return false;
        }
        if (b >= subst_.size())
            subst_.reserve(b + 1);
        while (subst_.size() <= b)
            subst_.push(kInvalidType);
        subst_[b] = a;
        return true;
    }

    if (kind_a != kind_b) {
        diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                      "type mismatch: cannot unify types of different kinds", span);
        return false;
    }

    auto &data_a = intern_.lookup(a);
    auto &data_b = intern_.lookup(b);

    switch (kind_a) {
    case TypeKind::Int:
        if (std::get<TypeInt>(data_a).width != std::get<TypeInt>(data_b).width) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                          "type mismatch: integer width differs", span);
            return false;
        }
        return true;
    case TypeKind::Float:
        if (std::get<TypeFloat>(data_a).width != std::get<TypeFloat>(data_b).width) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                          "type mismatch: float width differs", span);
            return false;
        }
        return true;
    case TypeKind::Ptr:
        return unify(std::get<TypePtr>(data_a).pointee, std::get<TypePtr>(data_b).pointee, span);
    case TypeKind::Array: {
        auto &arr_a = std::get<TypeArray>(data_a);
        auto &arr_b = std::get<TypeArray>(data_b);
        if (arr_a.count != arr_b.count) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                          "type mismatch: array length differs", span);
            return false;
        }
        return unify(arr_a.elem, arr_b.elem, span);
    }
    case TypeKind::Struct:
        if (std::get<TypeStruct>(data_a).def_id != std::get<TypeStruct>(data_b).def_id) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                          "type mismatch: different struct types", span);
            return false;
        }
        return true;
    case TypeKind::Fn: {
        auto &fn_a = std::get<TypeFn>(data_a);
        auto &fn_b = std::get<TypeFn>(data_b);
        if (fn_a.params.size() != fn_b.params.size()) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::TypeMismatch,
                          "type mismatch: function parameter count differs", span);
            return false;
        }
        if (!unify(fn_a.ret, fn_b.ret, span))
            return false;
        for (size_t i = 0; i < fn_a.params.size(); i++) {
            if (!unify(fn_a.params[i], fn_b.params[i], span))
                return false;
        }
        return true;
    }
    case TypeKind::Optional:
        return unify(std::get<TypeOptional>(data_a).inner, std::get<TypeOptional>(data_b).inner, span);
    case TypeKind::Failable:
        return unify(std::get<TypeFailable>(data_a).inner, std::get<TypeFailable>(data_b).inner, span);
    default:
        return true;
    }
}

bool Unifier::isAssignable(TypeId dst, TypeId src) const {
    dst = substitute(dst);
    src = substitute(src);

    if (dst == src)
        return true;

    auto kind_dst = intern_.kindOf(dst);
    auto kind_src = intern_.kindOf(src);

    if (kind_dst == TypeKind::TypeVar || kind_src == TypeKind::TypeVar)
        return true;

    if (kind_dst != kind_src) {
        if (kind_src == TypeKind::Never)
            return true;
        if (kind_dst == TypeKind::Bool && kind_src == TypeKind::Int) {
            auto &src_int = std::get<TypeInt>(intern_.lookup(src));
            return src_int.width == IntWidth::I8 || src_int.width == IntWidth::U8;
        }
        return false;
    }

    auto &data_dst = intern_.lookup(dst);
    auto &data_src = intern_.lookup(src);

    switch (kind_dst) {
    case TypeKind::Int:
        return std::get<TypeInt>(data_dst).width == std::get<TypeInt>(data_src).width;
    case TypeKind::Float:
        return std::get<TypeFloat>(data_dst).width == std::get<TypeFloat>(data_src).width;
    case TypeKind::Ptr:
        return isAssignable(std::get<TypePtr>(data_dst).pointee,
                            std::get<TypePtr>(data_src).pointee);
    case TypeKind::Array: {
        auto &arr_dst = std::get<TypeArray>(data_dst);
        auto &arr_src = std::get<TypeArray>(data_src);
        if (arr_dst.count != arr_src.count && arr_dst.count != 0 && arr_src.count != 0)
            return false;
        return isAssignable(arr_dst.elem, arr_src.elem);
    }
    case TypeKind::Struct:
        return std::get<TypeStruct>(data_dst).def_id == std::get<TypeStruct>(data_src).def_id;
    case TypeKind::Fn: {
        auto &fn_dst = std::get<TypeFn>(data_dst);
        auto &fn_src = std::get<TypeFn>(data_src);
        if (fn_dst.params.size() != fn_src.params.size())
            return false;
        if (!isAssignable(fn_dst.ret, fn_src.ret))
            return false;
        for (size_t i = 0; i < fn_dst.params.size(); i++) {
            if (!isAssignable(fn_dst.params[i], fn_src.params[i]))
                return false;
        }
        return true;
    }
    case TypeKind::Optional:
        return isAssignable(std::get<TypeOptional>(data_dst).inner,
                            std::get<TypeOptional>(data_src).inner);
    case TypeKind::Failable:
        return isAssignable(std::get<TypeFailable>(data_dst).inner,
                            std::get<TypeFailable>(data_src).inner);
    case TypeKind::Never:
        return true;
    default:
        return kind_dst == kind_src;
    }
}

bool Unifier::isCoercible(TypeId dst, TypeId src) const {
    dst = substitute(dst);
    src = substitute(src);

    if (isAssignable(dst, src))
        return true;

    auto kind_dst = intern_.kindOf(dst);
    auto kind_src = intern_.kindOf(src);

    if (kind_dst == TypeKind::Optional) {
        auto inner = std::get<TypeOptional>(intern_.lookup(dst)).inner;
        return isCoercible(inner, src);
    }

    if (kind_dst == TypeKind::Int && kind_src == TypeKind::Int) {
        auto w_dst = std::get<TypeInt>(intern_.lookup(dst)).width;
        auto w_src = std::get<TypeInt>(intern_.lookup(src)).width;
        auto to_u = [](IntWidth w) -> uint8_t {
            switch (w) {
            case IntWidth::I8:  case IntWidth::U8:  return 8;
            case IntWidth::I16: case IntWidth::U16: return 16;
            case IntWidth::I32: case IntWidth::U32: return 32;
            case IntWidth::I64: case IntWidth::U64: return 64;
            case IntWidth::I128: case IntWidth::U128: return 128;
            }
            return 0;
        };
        return to_u(w_dst) >= to_u(w_src);
    }

    if (kind_dst == TypeKind::Float && kind_src == TypeKind::Float) {
        auto w_dst = std::get<TypeFloat>(intern_.lookup(dst)).width;
        auto w_src = std::get<TypeFloat>(intern_.lookup(src)).width;
        return w_dst >= w_src;
    }

    return kind_dst == kind_src;
}

} // namespace zith::types
