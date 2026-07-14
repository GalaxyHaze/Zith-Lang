#include "sema-pipeline.hpp"
#include "ast/ast-node-utils.hpp"
#include "diagnostics/error-codes.hpp"
#include "types/type-id.hpp"
#include "types/type-lower.hpp"

#include <cstdlib>
#include <string>

namespace zith::sema {

namespace {

using ast::LitKind;
using diagnostics::Severity;
using diagnostics::err::AmbiguousCall;
using diagnostics::err::NoMatchingFn;
using diagnostics::err::TypeMismatch;
using diagnostics::err::UndefinedIdent;
using diagnostics::err::WrongArity;

types::TypeId defaultTypeForLit(ast::LitKind kind, types::TypeIntern &types) {
    switch (kind) {
    case LitKind::Int:
        return types.internInt(types::IntWidth::Literal);
    case LitKind::Float:
        return types.internFloat(types::FloatWidth::F64);
    case LitKind::Bool:
        return types::kBoolType;
    case LitKind::Char:
        return types::kCharType;
    case LitKind::String:
        return types.internPtr(types::kCharType);
    case LitKind::Nil:
        return types::kVoidType;
    }
    return types::kErrorType;
}

unsigned intWidthBits(types::IntWidth w) {
    switch (w) {
    case types::IntWidth::I8:
    case types::IntWidth::U8:
        return 8;
    case types::IntWidth::I16:
    case types::IntWidth::U16:
        return 16;
    case types::IntWidth::I32:
    case types::IntWidth::U32:
        return 32;
    case types::IntWidth::I64:
    case types::IntWidth::U64:
        return 64;
    case types::IntWidth::I128:
    case types::IntWidth::U128:
        return 128;
    case types::IntWidth::Literal:
        return 0;
    }
    return 0;
}

bool isSignedWidth(types::IntWidth w) {
    return w >= types::IntWidth::I8 && w <= types::IntWidth::I128;
}

const char *checkIntOverflow(int64_t value, types::IntWidth target) {
    unsigned bits = intWidthBits(target);
    if (bits == 0)
        return nullptr;
    if (isSignedWidth(target)) {
        int64_t min = -(1LL << (bits - 1));
        int64_t max = (1LL << (bits - 1)) - 1;
        if (value < min || value > max)
            return "value does not fit in the target integer type";
    } else {
        uint64_t max = (bits >= 64) ? ~0ULL : (1ULL << bits) - 1;
        if (value < 0 || static_cast<uint64_t>(value) > max)
            return "value does not fit in the target unsigned integer type";
    }
    return nullptr;
}

} // namespace

SemaPipeline::SemaPipeline(symbols::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &arena, const memory::DynArray<symbols::SymId> *resolved,
                           symbols::ImportManager *import_mgr)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, arena), arena_(arena),
      resolved_(resolved), typed_ast_(arena), import_mgr_(import_mgr), worklist_(arena),
      active_builder_(&builder), allowed_files_(arena), var_types_(arena), checked_fns_(arena) {}

types::TypeId SemaPipeline::astExprType(ast::ExprId id) const {
    return typed_ast_.get(id);
}

ast::AstBuilder &SemaPipeline::builder() const {
    return *active_builder_;
}

bool SemaPipeline::isSymAccessible(symbols::SymId sym_id) const {
    if (allowed_files_.empty())
        return true;
    auto origin =
        import_mgr_ ? import_mgr_->originOf(sym_id) : memory::Optional<symbols::SymOrigin>{};
    if (!origin)
        return true;
    for (auto af : allowed_files_)
        if (af == origin->file_idx)
            return true;
    return false;
}

ast::AstBuilder *SemaPipeline::builderForSym(symbols::SymId fn_sym) {
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            return import_mgr_->get(origin->file_idx).builder;
    }
    return &ctx_.builder();
}

const symbols::SymbolTable *SemaPipeline::symsForSym(symbols::SymId fn_sym) {
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            return &import_mgr_->get(origin->file_idx).symbols;
    }
    return &ctx_.syms();
}

symbols::SymId SemaPipeline::localSymFor(symbols::SymId fn_sym) const {
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            return origin->local_sym;
    }
    return fn_sym;
}

const ast::FnDeclNode *SemaPipeline::fnDeclForSym(symbols::SymId fn_sym,
                                                  ast::AstBuilder **source_bld,
                                                  const symbols::SymbolTable **source_syms) {
    auto *bld  = builderForSym(fn_sym);
    auto *syms = symsForSym(fn_sym);
    if (source_bld)
        *source_bld = bld;
    if (source_syms)
        *source_syms = syms;
    if (!bld || !syms)
        return nullptr;

    auto local_sym   = localSymFor(fn_sym);
    const auto &data = syms->get(local_sym);
    if (data.kind != symbols::SymKind::Fn || data.decl_id == ast::kInvalidDecl)
        return nullptr;
    return ast::asFn(bld->getDecl(data.decl_id));
}

types::TypeId SemaPipeline::lowerFnReturnType(symbols::SymId fn_sym) {
    ast::AstBuilder *source_bld = nullptr;
    auto *fn                    = fnDeclForSym(fn_sym, &source_bld, nullptr);
    if (!fn || !source_bld)
        return types::kErrorType;
    types::TypeLower lower(*source_bld, ctx_.types(), ctx_.diags(), ctx_.syms());
    return lower.lower(fn->return_type);
}

types::TypeId SemaPipeline::lowerFnParamType(symbols::SymId fn_sym, size_t index) {
    ast::AstBuilder *source_bld = nullptr;
    auto *fn                    = fnDeclForSym(fn_sym, &source_bld, nullptr);
    if (!fn || !source_bld || index >= fn->params.size())
        return types::kErrorType;
    types::TypeLower lower(*source_bld, ctx_.types(), ctx_.diags(), ctx_.syms());
    return lower.lower(fn->params[index].type);
}

void SemaPipeline::ensureVarTypeCapacity(symbols::SymId sym_id) {
    if (sym_id >= var_types_.capacity())
        var_types_.reserve(sym_id + 1);
    while (var_types_.size() <= sym_id)
        var_types_.push(types::kErrorType);
}

void SemaPipeline::ensureCheckedCapacity(symbols::SymId sym_id) {
    if (sym_id >= checked_fns_.capacity())
        checked_fns_.reserve(sym_id + 1);
    while (checked_fns_.size() <= sym_id)
        checked_fns_.push(0);
}

bool SemaPipeline::isBodyChecked(symbols::SymId sym_id) const {
    return sym_id < checked_fns_.size() && checked_fns_[sym_id] != 0;
}

void SemaPipeline::markBodyChecked(symbols::SymId sym_id) {
    ensureCheckedCapacity(sym_id);
    checked_fns_[sym_id] = 1;
}

bool SemaPipeline::concretizeLiteralExpr(ast::ExprId id, types::TypeId concrete_type,
                                         memory::Span span) {
    if (id == ast::kInvalidExpr || concrete_type == types::kErrorType)
        return false;

    auto &node = builder().getExpr(id);
    if (ast::exprKind(node) != ast::ExprKind::Literal)
        return false;
    auto &lit = std::get<ast::LitValue>(node);
    if (lit.kind != ast::LitKind::Int)
        return false;

    auto current_type = astExprType(id);
    if (current_type == types::kErrorType ||
        ctx_.types().kindOf(current_type) != types::TypeKind::Int)
        return false;

    auto &current_int = std::get<types::TypeInt>(ctx_.types().lookup(current_type));
    if (current_int.width != types::IntWidth::Literal)
        return false;

    typed_ast_.set(id, concrete_type);

    if (ctx_.types().kindOf(concrete_type) != types::TypeKind::Int)
        return true;

    auto &int_t = std::get<types::TypeInt>(ctx_.types().lookup(concrete_type));
    auto value  = std::strtoll(std::string(lit.raw).data(), nullptr, 10);
    if (auto *err = checkIntOverflow(value, int_t.width))
        ctx_.diags().report(Severity::Error, TypeMismatch, err, span);
    return true;
}

ast::ExprId SemaPipeline::implicitReturnExpr(ast::ExprId body_id) const {
    if (body_id == ast::kInvalidExpr)
        return ast::kInvalidExpr;
    auto &node = builder().getExpr(body_id);
    if (ast::exprKind(node) == ast::ExprKind::Block)
        return std::get<ast::BlockNode>(node).trailing;
    return body_id;
}

void SemaPipeline::ensureBodyChecked(symbols::SymId fn_sym) {
    if (isBodyChecked(fn_sym))
        return;
    markBodyChecked(fn_sym);

    ast::AstBuilder *source_bld             = nullptr;
    const symbols::SymbolTable *source_syms = nullptr;
    auto *fn                                = fnDeclForSym(fn_sym, &source_bld, &source_syms);
    if (!fn || !source_bld || !source_syms || fn->is_extern || fn->body == ast::kInvalidExpr)
        return;

    auto previous_allowed = std::move(allowed_files_);
    allowed_files_.clear();
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin) {
            allowed_files_.push(origin->file_idx);
            auto &file = import_mgr_->get(origin->file_idx);
            for (auto dep : file.dep_files)
                allowed_files_.push(dep);
        } else {
            for (auto dep : import_mgr_->rootDeps())
                allowed_files_.push(dep);
        }
    }

    auto *previous_builder = active_builder_;
    auto previous_ret      = current_fn_ret_type_;
    active_builder_        = source_bld;
    current_fn_ret_type_   = lowerFnReturnType(fn_sym);
    labelMap_              = {};

    auto scope = syms().enterScope();
    syms().emplace(*source_syms, scope);

    types::TypeLower lower(*source_bld, ctx_.types(), ctx_.diags(), ctx_.syms());
    for (const auto &param : fn->params) {
        auto param_sym = syms().declareInScope(scope, param.name);
        ensureVarTypeCapacity(param_sym);
        var_types_[param_sym] = lower.lower(param.type);
    }

    auto body_type        = visitExpr(fn->body);
    auto implicit_expr_id = implicitReturnExpr(fn->body);
    if (implicit_expr_id != ast::kInvalidExpr && current_fn_ret_type_ != types::kErrorType &&
        body_type != types::kErrorType) {
        if (unifier_.unify(body_type, current_fn_ret_type_, fn->span)) {
            types::TypeId concrete = current_fn_ret_type_;
            if (ctx_.types().kindOf(current_fn_ret_type_) == types::TypeKind::Int) {
                auto &type = std::get<types::TypeInt>(ctx_.types().lookup(current_fn_ret_type_));
                if (type.width == types::IntWidth::Literal)
                    concrete = body_type;
            }
            concretizeLiteralExpr(implicit_expr_id, concrete, fn->span);
        }
    }

    syms().exitScope();
    current_fn_ret_type_ = previous_ret;
    active_builder_      = previous_builder;
    allowed_files_       = std::move(previous_allowed);
}

bool SemaPipeline::run(const ast::ProgramNode &program) {
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *sd = ast::asStruct(decl))
            ctx_.types().registerNamedType(sd->name, types::TypeKind::Struct);
        else if (auto *ed = ast::asEnum(decl))
            ctx_.types().registerNamedType(ed->name, types::TypeKind::Enum);
        else if (auto *ud = ast::asUnion(decl))
            ctx_.types().registerNamedType(ud->name, types::TypeKind::Union);
        else if (auto *ad = ast::asTypeAlias(decl)) {
            types::TypeLower lower(ctx_.builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            auto target = lower.lower(ad->target_type);
            ctx_.types().registerTypeAlias(ad->name, target);
        }
    }

    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (!ast::asFn(decl))
            continue;
        for (symbols::SymId sym = 0; sym < ctx_.syms().symbolCount(); ++sym) {
            const auto &data = ctx_.syms().get(sym);
            if (data.kind == symbols::SymKind::Fn && data.decl_id == decl_id &&
                (!import_mgr_ || !import_mgr_->originOf(sym))) {
                ensureBodyChecked(sym);
                break;
            }
        }
    }

    for (size_t i = 0; i < worklist_.size(); ++i)
        ensureBodyChecked(worklist_[i]);

    return !ctx_.diags().hasErrors();
}

types::TypeId SemaPipeline::visitExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return types::kErrorType;

    auto &node = builder().getExpr(id);
    switch (ast::exprKind(node)) {
    case ast::ExprKind::Literal:
        return visitLiteral(id, std::get<ast::LitValue>(node));
    case ast::ExprKind::Identifier:
        return visitIdent(std::get<ast::IdentNode>(node), id);
    case ast::ExprKind::Binary:
        return visitBinary(id, std::get<ast::BinaryNode>(node));
    case ast::ExprKind::Unary:
        return visitUnary(id, std::get<ast::UnaryNode>(node));
    case ast::ExprKind::Call:
        return visitCall(id, std::get<ast::CallNode>(node));
    case ast::ExprKind::Block:
        return visitBlock(id, std::get<ast::BlockNode>(node));
    case ast::ExprKind::If:
        return visitIf(id, std::get<ast::IfNode>(node));
    case ast::ExprKind::While:
        return visitWhile(id, std::get<ast::WhileNode>(node));
    case ast::ExprKind::Field:
    case ast::ExprKind::Index:
    case ast::ExprKind::Range:
    case ast::ExprKind::Unbody:
    case ast::ExprKind::Intrinsic:
    case ast::ExprKind::MacroCall:
        typed_ast_.set(id, types::kErrorType);
        return types::kErrorType;
    }

    typed_ast_.set(id, types::kErrorType);
    return types::kErrorType;
}

types::TypeId SemaPipeline::visitLiteral(ast::ExprId id, const ast::LitValue &n) {
    auto default_type = defaultTypeForLit(n.kind, ctx_.types());
    typed_ast_.set(id, default_type);
    return default_type;
}

types::TypeId SemaPipeline::visitIdent(const ast::IdentNode &n, ast::ExprId id) {
    symbols::SymId sym = symbols::kInvalidSym;
    if (active_builder_ == &ctx_.builder() && resolved_ && id < resolved_->size())
        sym = (*resolved_)[id];
    if (sym == symbols::kInvalidSym)
        sym = ctx_.syms().lookup(n.name);
    if (sym != symbols::kInvalidSym && !isSymAccessible(sym)) {
        ctx_.diags().report(Severity::Error, UndefinedIdent,
                            "undefined identifier '" + std::string(n.name) + "'", n.span);
        sym = symbols::kInvalidSym;
    }
    if (sym == symbols::kInvalidSym) {
        const bool main_builder = active_builder_ == &ctx_.builder();
        if (!main_builder || !resolved_ || id >= resolved_->size() ||
            (*resolved_)[id] != symbols::kInvalidSym)
            ctx_.diags().report(Severity::Error, UndefinedIdent,
                                std::string("undefined identifier '") + std::string(n.name) + "'",
                                n.span);
        typed_ast_.set(id, types::kErrorType);
        return types::kErrorType;
    }

    auto &data               = ctx_.syms().get(sym);
    types::TypeId ident_type = types::kErrorType;
    if (data.kind == symbols::SymKind::Fn) {
        ident_type = ctx_.types().internUnknown();
    } else if (data.kind == symbols::SymKind::Variable && sym < var_types_.size()) {
        ident_type = var_types_[sym];
    }

    typed_ast_.set(id, ident_type);
    return ident_type;
}

types::TypeId SemaPipeline::visitBinary(ast::ExprId id, const ast::BinaryNode &n) {
    auto lhs_type = visitExpr(n.lhs);
    auto rhs_type = visitExpr(n.rhs);
    if (lhs_type == types::kErrorType || rhs_type == types::kErrorType) {
        typed_ast_.set(id, types::kErrorType);
        return types::kErrorType;
    }

    if (!unifier_.unify(lhs_type, rhs_type, n.span)) {
        typed_ast_.set(id, types::kErrorType);
        return types::kErrorType;
    }

    types::TypeId result_type = lhs_type;
    types::TypeId concrete    = result_type;
    if (ctx_.types().kindOf(result_type) == types::TypeKind::Int) {
        auto &type = std::get<types::TypeInt>(ctx_.types().lookup(result_type));
        if (type.width == types::IntWidth::Literal)
            concrete = rhs_type;
    }

    concretizeLiteralExpr(n.lhs, concrete, n.span);
    concretizeLiteralExpr(n.rhs, concrete, n.span);
    if (ctx_.types().kindOf(result_type) == types::TypeKind::Int) {
        auto &type = std::get<types::TypeInt>(ctx_.types().lookup(result_type));
        if (type.width == types::IntWidth::Literal)
            result_type = concrete;
    }

    typed_ast_.set(id, result_type);
    return result_type;
}

types::TypeId SemaPipeline::visitUnary(ast::ExprId id, const ast::UnaryNode &n) {
    auto operand_type = visitExpr(n.operand);
    typed_ast_.set(id, operand_type);
    return operand_type;
}

types::TypeId SemaPipeline::visitCall(ast::ExprId id, const ast::CallNode &n) {
    auto callee_type = visitExpr(n.callee);

    memory::DynArray<types::TypeId> arg_types{arena_};
    for (auto arg : n.args)
        arg_types.push(visitExpr(arg));

    symbols::SymId resolved_fn = symbols::kInvalidSym;
    types::TypeId result_type  = types::kErrorType;
    size_t match_count         = 0;

    auto &callee_expr = builder().getExpr(n.callee);
    if (ast::exprKind(callee_expr) == ast::ExprKind::Identifier) {
        auto callee_name          = std::get<ast::IdentNode>(callee_expr).name;
        auto candidates           = syms().lookupAll(callee_name, arena_);
        size_t fn_candidate_count = 0;

        for (auto sym_id : candidates) {
            auto &data = syms().get(sym_id);
            if (data.kind != symbols::SymKind::Fn)
                continue;
            if (!isSymAccessible(sym_id))
                continue;
            fn_candidate_count++;

            auto *fn_decl = fnDeclForSym(sym_id, nullptr, nullptr);
            if (!fn_decl || fn_decl->params.size() != n.args.size())
                continue;

            bool match = true;
            for (size_t i = 0; i < fn_decl->params.size(); ++i) {
                auto param_type = lowerFnParamType(sym_id, i);
                if (param_type == types::kErrorType || i >= arg_types.size() ||
                    arg_types[i] == types::kErrorType)
                    continue;
                if (!unifier_.isCoercible(param_type, arg_types[i])) {
                    match = false;
                    break;
                }
            }

            if (match) {
                match_count++;
                if (match_count == 1) {
                    resolved_fn = sym_id;
                    result_type = lowerFnReturnType(sym_id);
                    if (!fn_decl->is_extern && fn_decl->body != ast::kInvalidExpr)
                        worklist_.push(sym_id);
                }
            }
        }

        if (match_count == 0 && fn_candidate_count > 0) {
            if (!n.args.empty()) {
                ctx_.diags().report(
                    Severity::Error, NoMatchingFn,
                    "no matching function for call to '" + std::string(callee_name) + "'", n.span);
            } else {
                ctx_.diags().report(Severity::Error, WrongArity,
                                    "wrong number of arguments in call", n.span);
            }
        } else if (match_count > 1) {
            ctx_.diags().report(Severity::Error, AmbiguousCall,
                                "ambiguous call '" + std::string(callee_name) +
                                    "' — multiple functions match",
                                n.span);
            resolved_fn = symbols::kInvalidSym;
            result_type = types::kErrorType;
        }
    }

    if (resolved_fn != symbols::kInvalidSym) {
        auto *fn_decl = fnDeclForSym(resolved_fn, nullptr, nullptr);
        if (fn_decl) {
            for (size_t i = 0; i < fn_decl->params.size() && i < n.args.size(); ++i) {
                auto param_type = lowerFnParamType(resolved_fn, i);
                if (param_type != types::kErrorType && i < arg_types.size() &&
                    arg_types[i] != types::kErrorType)
                    concretizeLiteralExpr(n.args[i], param_type, n.span);
            }
        }
    } else if (callee_type != types::kErrorType) {
        auto &fn_type = ctx_.types().lookup(callee_type);
        auto fn_ptr   = std::get_if<types::TypeFn>(&fn_type);
        if (fn_ptr) {
            if (arg_types.size() != fn_ptr->param_count) {
                ctx_.diags().report(Severity::Error, WrongArity,
                                    "wrong number of arguments in call", {});
            } else {
                for (size_t i = 0; i < arg_types.size(); ++i) {
                    if (arg_types[i] != types::kErrorType)
                        unifier_.unify(arg_types[i], fn_ptr->params[i], n.span);
                    concretizeLiteralExpr(n.args[i], fn_ptr->params[i], n.span);
                }
                result_type = fn_ptr->ret;
            }
        }
    }

    typed_ast_.set(id, result_type);
    return result_type;
}

types::TypeId SemaPipeline::visitBlock(ast::ExprId id, const ast::BlockNode &n) {
    for (auto stmt_id : n.stmts)
        visitStmt(stmt_id);

    types::TypeId result_type = types::kVoidType;
    if (n.trailing != ast::kInvalidExpr)
        result_type = visitExpr(n.trailing);

    typed_ast_.set(id, result_type);
    return result_type;
}

types::TypeId SemaPipeline::visitIf(ast::ExprId id, const ast::IfNode &n) {
    visitExpr(n.cond);

    auto then_type = visitExpr(n.then_branch);
    if (n.else_branch == ast::kInvalidExpr) {
        typed_ast_.set(id, types::kVoidType);
        return types::kVoidType;
    }

    auto else_type = visitExpr(n.else_branch);
    if (then_type == types::kErrorType || else_type == types::kErrorType ||
        !unifier_.unify(then_type, else_type, n.span)) {
        typed_ast_.set(id, types::kErrorType);
        return types::kErrorType;
    }

    types::TypeId result_type = then_type;
    types::TypeId concrete    = result_type;
    if (ctx_.types().kindOf(result_type) == types::TypeKind::Int) {
        auto &type = std::get<types::TypeInt>(ctx_.types().lookup(result_type));
        if (type.width == types::IntWidth::Literal)
            concrete = else_type;
    }

    concretizeLiteralExpr(implicitReturnExpr(n.then_branch), concrete, n.span);
    concretizeLiteralExpr(implicitReturnExpr(n.else_branch), concrete, n.span);
    if (ctx_.types().kindOf(result_type) == types::TypeKind::Int) {
        auto &type = std::get<types::TypeInt>(ctx_.types().lookup(result_type));
        if (type.width == types::IntWidth::Literal)
            result_type = concrete;
    }

    typed_ast_.set(id, result_type);
    return result_type;
}

types::TypeId SemaPipeline::visitWhile(ast::ExprId id, const ast::WhileNode &n) {
    visitExpr(n.cond);
    visitExpr(n.body);
    typed_ast_.set(id, types::kVoidType);
    return types::kVoidType;
}

void SemaPipeline::visitMarker(const ast::MarkerNode &n) {
    labelMap_.insert(n.name, labelMap_.size());
    for (auto stmt_id : n.body)
        visitStmt(stmt_id);
}

void SemaPipeline::visitGoto(const ast::GotoNode &n) {
    if (!labelMap_.get(n.target)) {
        ctx_.diags().report(Severity::Error, UndefinedIdent,
                            "undefined marker '" + std::string(n.target) + "'", n.span);
    }
}

void SemaPipeline::visitStmt(ast::StmtId id) {
    if (id == ast::kInvalidStmt)
        return;

    auto &node = builder().getStmt(id);
    switch (ast::stmtKind(node)) {
    case ast::StmtKind::Let: {
        const auto &n = std::get<ast::LetNode>(node);

        types::TypeId decl_type = types::kErrorType;
        if (n.type_annot != ast::kInvalidTypeExpr) {
            types::TypeLower lower(builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            decl_type = lower.lower(n.type_annot);
        }

        types::TypeId init_type = types::kErrorType;
        if (n.init != ast::kInvalidExpr)
            init_type = visitExpr(n.init);

        if (n.type_annot != ast::kInvalidTypeExpr && n.init != ast::kInvalidExpr &&
            decl_type != types::kErrorType && init_type != types::kErrorType &&
            unifier_.unify(decl_type, init_type, n.span)) {
            types::TypeId concrete = decl_type;
            if (ctx_.types().kindOf(decl_type) == types::TypeKind::Int) {
                auto &type = std::get<types::TypeInt>(ctx_.types().lookup(decl_type));
                if (type.width == types::IntWidth::Literal)
                    concrete = init_type;
            }
            concretizeLiteralExpr(n.init, concrete, n.span);
        }

        types::TypeId var_type = init_type;
        if (n.type_annot != ast::kInvalidTypeExpr && decl_type != types::kErrorType)
            var_type = decl_type;
        else if (init_type != types::kErrorType)
            var_type = init_type;

        for (auto var_name : n.names) {
            auto sym_id =
                syms().declare(var_name, symbols::SymbolVisibility::Private, 0,
                               symbols::SymKind::Variable, ast::kInvalidDecl, memory::Span{});
            if (sym_id != symbols::kInvalidSym) {
                ensureVarTypeCapacity(sym_id);
                var_types_[sym_id] = var_type;
            }
        }
        break;
    }
    case ast::StmtKind::Assign: {
        const auto &n = std::get<ast::AssignNode>(node);
        visitExpr(n.target);
        visitExpr(n.value);
        break;
    }
    case ast::StmtKind::Return: {
        const auto &n = std::get<ast::RetNode>(node);
        auto val_type = types::kVoidType;
        if (n.value != ast::kInvalidExpr)
            val_type = visitExpr(n.value);

        if (val_type != types::kErrorType && current_fn_ret_type_ != types::kErrorType &&
            unifier_.unify(val_type, current_fn_ret_type_, n.span)) {
            types::TypeId concrete = current_fn_ret_type_;
            if (ctx_.types().kindOf(current_fn_ret_type_) == types::TypeKind::Int) {
                auto &type = std::get<types::TypeInt>(ctx_.types().lookup(current_fn_ret_type_));
                if (type.width == types::IntWidth::Literal)
                    concrete = val_type;
            }
            concretizeLiteralExpr(n.value, concrete, n.span);
        }
        break;
    }
    case ast::StmtKind::Goto:
        visitGoto(std::get<ast::GotoNode>(node));
        break;
    case ast::StmtKind::Marker:
        visitMarker(std::get<ast::MarkerNode>(node));
        break;
    case ast::StmtKind::Expr:
        visitExpr(std::get<ast::ExprStmtNode>(node).expr);
        break;
    }
}

} // namespace zith::sema
