#include "type-lower.hpp"
#include "diagnostics/error-codes.hpp"

#include <cstring>
#include <span>

namespace zith::types {

namespace err = diagnostics::err;

TypeLower::TypeLower(ast::AstBuilder &ast, TypeIntern &intern,
                     diagnostics::DiagnosticEngine &diags,
                     const import::SymbolTable &syms)
    : ast_(ast), intern_(intern), diags_(diags), syms_(syms) {}

void TypeLower::setGenericContext(const memory::FlatMap<std::string_view, TypeId> *ctx) {
    generic_ctx_ = ctx;
}

TypeId TypeLower::lower(ast::TypeExprId id) {
    if (id == ast::kInvalidTypeExpr)
        return kErrorType;
    return lowerNode(ast_.getTypeExpr(id));
}

TypeId TypeLower::lowerNode(const ast::TypeExprNode &node) {
    struct Visitor {
        TypeLower &self;
        ast::AstBuilder &ast;

        TypeId operator()(const ast::TypePath &n) {
            // Single-segment path: look up name in symbol table
            if (n.segments.size() == 1) {
                auto sym_id = self.syms_.lookup(n.segments[0]);
                if (sym_id == import::kInvalidSym) {
                    self.diags_.report(diagnostics::Severity::Error,
                        diagnostics::err::UndefinedIdent,
                        "undefined type '" + std::string(n.segments[0]) + "'",
                        n.span);
                    return kErrorType;
                }
                auto &data = self.syms_.get(sym_id);
                switch (data.kind) {
                case import::SymKind::Struct:
                case import::SymKind::Enum:
                case import::SymKind::Union:
                case import::SymKind::Trait:
                case import::SymKind::Interface:
                case import::SymKind::Alias:
                    // Known type — return unknown so unification handles init checks
                    return self.intern_.internUnknown();
                default:
                    self.diags_.report(diagnostics::Severity::Error,
                        diagnostics::err::UndefinedIdent,
                        "'" + std::string(n.segments[0]) + "' is not a type",
                        n.span);
                    return kErrorType;
                }
            }
            // Multi-segment path (qualified) — not supported yet
            self.diags_.report(diagnostics::Severity::Error, diagnostics::err::CannotInfer,
                               "unresolved type path", {});
            return kErrorType;
        }

        TypeId operator()(const ast::TypeBuiltin &n) {
            switch (n.kind) {
            case ast::BuiltinType::I8:    return self.intern_.internInt(IntWidth::I8);
            case ast::BuiltinType::I16:   return self.intern_.internInt(IntWidth::I16);
            case ast::BuiltinType::I32:   return self.intern_.internInt(IntWidth::I32);
            case ast::BuiltinType::I64:   return self.intern_.internInt(IntWidth::I64);
            case ast::BuiltinType::I128:  return self.intern_.internInt(IntWidth::I128);
            case ast::BuiltinType::U8:    return self.intern_.internInt(IntWidth::U8);
            case ast::BuiltinType::U16:   return self.intern_.internInt(IntWidth::U16);
            case ast::BuiltinType::U32:   return self.intern_.internInt(IntWidth::U32);
            case ast::BuiltinType::U64:   return self.intern_.internInt(IntWidth::U64);
            case ast::BuiltinType::U128:  return self.intern_.internInt(IntWidth::U128);
            case ast::BuiltinType::F32:   return self.intern_.internFloat(FloatWidth::F32);
            case ast::BuiltinType::F64:   return self.intern_.internFloat(FloatWidth::F64);
            case ast::BuiltinType::Bool:  return kBoolType;
            case ast::BuiltinType::Char:  return kCharType;
            case ast::BuiltinType::Void:  return kVoidType;
            case ast::BuiltinType::Never: return kNeverType;
            case ast::BuiltinType::Unknown: return self.intern_.internUnknown();
            case ast::BuiltinType::Invalid: return kErrorType;
            case ast::BuiltinType::Opaque:  return self.intern_.intern(TypeOpaque{});
            }
            return kErrorType;
        }

        TypeId operator()(const ast::TypePtrExpr &n) {
            auto pointee = self.lower(n.pointee);
            auto ownership = static_cast<OwnershipKind>(n.ownership);
            return self.intern_.internPtr(pointee, n.is_mut, ownership);
        }

        TypeId operator()(const ast::TypeSlice &n) {
            return self.intern_.internSlice(self.lower(n.elem));
        }

        TypeId operator()(const ast::TypeArray &n) {
            // count is currently a placeholder TypeExprId — skip for now
            return self.intern_.internArray(self.lower(n.elem), 0);
        }

        TypeId operator()(const ast::TypeFnExpr &n) {
            auto ret = self.lower(n.ret);
            memory::DynArray<TypeId> params{ast.arena()};
            for (auto p : n.params)
                params.push(self.lower(p));
            return self.intern_.internFn(std::span<const TypeId>{params.data(), params.size()}, ret);
        }

        TypeId operator()(const ast::TypeOptional &n) {
            return self.intern_.internOptional(self.lower(n.inner));
        }

        TypeId operator()(const ast::TypeFailable &n) {
            auto inner = self.lower(n.inner);
            bool has_error = n.error != ast::kInvalidTypeExpr && n.error != self.ast_.inferExpr();
            if (has_error) {
                auto err = self.lower(n.error);
                // Failable with explicit error type — store as pair
                // For now: just store inner
                return self.intern_.internFailable(inner);
            }
            return self.intern_.internFailable(inner);
        }

        TypeId operator()(const ast::TypeApp &n) {
            auto base = self.lower(n.base);
            memory::DynArray<TypeId> args{ast.arena()};
            for (auto a : n.args)
                args.push(self.lower(a));
            return self.intern_.internIncomplete(base,
                std::span<const TypeId>{args.data(), args.size()});
        }

        TypeId operator()(const ast::TypePack &n) {
            memory::DynArray<TypeId> members{ast.arena()};
            memory::DynArray<std::string_view> names{ast.arena()};
            for (auto &m : n.members) {
                members.push(self.lower(m.type));
                names.push(m.name);
            }
            return self.intern_.internPack(
                std::span<const TypeId>{members.data(), members.size()},
                std::span<const std::string_view>{names.data(), names.size()});
        }

        TypeId operator()(const ast::TypeSum &n) {
            memory::DynArray<TypeId> members{ast.arena()};
            for (auto m : n.members)
                members.push(self.lower(m));
            return self.intern_.internSum(
                std::span<const TypeId>{members.data(), members.size()});
        }

        TypeId operator()(const ast::TypeInfer &) {
            // Inferred type — leave unresolved for sema
            return self.intern_.internUnknown();
        }

        TypeId operator()(const ast::TypeGenericParamRef &n) {
            if (self.generic_ctx_) {
                auto *val = self.generic_ctx_->get(n.name);
                if (val) return *val;
            }
            self.diags_.report(diagnostics::Severity::Error, diagnostics::err::UndefinedIdent,
                               "unknown generic parameter '" + std::string(n.name) + "'", {});
            return kErrorType;
        }
    };

    return std::visit(Visitor{*this, ast_}, node);
}

} // namespace zith::types
