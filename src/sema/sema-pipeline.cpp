#include "sema-pipeline.hpp"
#include "diagnostics/error-codes.hpp"
#include "symbols/symbol-id.hpp"
#include "types/type-id.hpp"

#include <cstdlib>
#include <string>

namespace zith::sema {

namespace {

using ast::BinaryOp;
using ast::LitKind;
using ast::UnaryOp;
using diagnostics::Severity;
using diagnostics::err::AmbiguousCall;
using diagnostics::err::NoMatchingFn;
using diagnostics::err::TypeMismatch;
using diagnostics::err::UndefinedIdent;
using diagnostics::err::WrongArity;

hir::HirBinaryOp mapBinaryOp(ast::BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:
        return hir::HirBinaryOp::Add;
    case BinaryOp::Sub:
        return hir::HirBinaryOp::Sub;
    case BinaryOp::Mul:
        return hir::HirBinaryOp::Mul;
    case BinaryOp::Div:
        return hir::HirBinaryOp::Div;
    case BinaryOp::Rest:
        return hir::HirBinaryOp::Rem;
    case BinaryOp::Eq:
        return hir::HirBinaryOp::Eq;
    case BinaryOp::Ne:
        return hir::HirBinaryOp::Ne;
    case BinaryOp::Lt:
        return hir::HirBinaryOp::Lt;
    case BinaryOp::Le:
        return hir::HirBinaryOp::Le;
    case BinaryOp::Gt:
        return hir::HirBinaryOp::Gt;
    case BinaryOp::Ge:
        return hir::HirBinaryOp::Ge;
    case BinaryOp::And:
        return hir::HirBinaryOp::And;
    case BinaryOp::Or:
        return hir::HirBinaryOp::Or;
    case BinaryOp::Xor:
        return hir::HirBinaryOp::Xor;
    default:
        return hir::HirBinaryOp::Add;
    }
}

hir::HirUnaryOp mapUnaryOp(ast::UnaryOp op) {
    switch (op) {
    case UnaryOp::Neg:
        return hir::HirUnaryOp::Neg;
    case UnaryOp::Not:
        return hir::HirUnaryOp::Not;
    case UnaryOp::Ref:
        return hir::HirUnaryOp::Ref;
    case UnaryOp::Deref:
        return hir::HirUnaryOp::Deref;
    }
    return hir::HirUnaryOp::Neg;
}

types::TypeId defaultTypeForLit(ast::LitKind kind, types::TypeIntern &types) {
    switch (kind) {
    case LitKind::Int:
        return types.internInt(types::IntWidth::I64);
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

} // anonymous namespace

SemaPipeline::SemaPipeline(symbols::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &hir_arena,
                           const memory::DynArray<symbols::SymId> *resolved,
                           symbols::ImportManager *import_mgr)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_arena_(hir_arena),
      hir_(hir_arena), resolved_(resolved), import_mgr_(import_mgr), worklist_(hir_arena),
      fn_syms_(hir_arena), active_builder_(&builder), hir_types_(hir_arena),
      allowed_files_(hir_arena) {}

bool SemaPipeline::isSymAccessible(symbols::SymId sym_id) const {
    if (allowed_files_.empty())
        return true;
    auto origin = import_mgr_ ? import_mgr_->originOf(sym_id)
                              : memory::Optional<symbols::ImportManager::SymOrigin>{};
    if (!origin)
        return true;
    for (auto af : allowed_files_)
        if (af == origin->file_idx)
            return true;
    return false;
}

ast::AstBuilder &SemaPipeline::builder() const {
    return *active_builder_;
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

size_t SemaPipeline::hirIndexForSym(symbols::SymId fn_sym) const {
    for (size_t i = 0; i < fn_syms_.size(); ++i)
        if (fn_syms_[i] == fn_sym)
            return i;
    return static_cast<size_t>(-1);
}

void SemaPipeline::ensureStub(symbols::SymId fn_sym) {
    if (hirIndexForSym(fn_sym) != static_cast<size_t>(-1))
        return;
    auto *source_syms = symsForSym(fn_sym);
    auto *source_bld  = builderForSym(fn_sym);
    if (!source_syms || !source_bld)
        return;
    auto local_sym = fn_sym;
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            local_sym = origin->local_sym;
    }
    const auto &data = source_syms->get(local_sym);
    if (data.kind != symbols::SymKind::Fn || data.decl_id == ast::kInvalidDecl)
        return;
    auto *fn = std::get_if<ast::FnDeclNode>(&source_bld->getDecl(data.decl_id));
    if (!fn)
        return;

    auto &hfn   = hir_.addFn(data.name);
    hfn.decl_id = data.decl_id;
    types::TypeLower lower(*source_bld, ctx_.types(), ctx_.diags(), ctx_.syms());
    for (const auto &param : fn->params)
        hfn.params.push(lower.lower(param.type));
    hfn.return_type = lower.lower(fn->return_type);
    fn_syms_.push(fn_sym);
}

void SemaPipeline::ensureBodyLowered(symbols::SymId fn_sym) {
    ensureStub(fn_sym);
    auto idx = hirIndexForSym(fn_sym);
    if (idx == static_cast<size_t>(-1))
        return;
    auto &hfn = hir_.getFn(idx);
    if (!hfn.blocks.empty())
        return;
    auto *source_bld  = builderForSym(fn_sym);
    auto *source_syms = symsForSym(fn_sym);
    if (!source_bld || !source_syms)
        return;
    auto local_sym = fn_sym;
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            local_sym = origin->local_sym;
    }
    const auto &data = source_syms->get(local_sym);
    auto *fn         = std::get_if<ast::FnDeclNode>(&source_bld->getDecl(data.decl_id));
    if (!fn || fn->is_extern || fn->body == ast::kInvalidExpr)
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
        }
    }

    auto *previous_builder = active_builder_;
    auto *previous_fn      = current_fn_;
    auto previous_ret      = current_fn_ret_type_;
    active_builder_        = source_bld;
    current_fn_            = &hfn;
    current_fn_ret_type_   = hfn.return_type;
    auto scope             = syms().enterScope();
    syms().emplace(*source_syms, scope);
    for (const auto &param : fn->params)
        syms().declareInScope(scope, param.name);
    auto &entry = hfn.blocks.emplace(hir_arena_);
    entry.insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    auto body   = visitExpr(fn->body);
    if (body != hir::kInvalidHirExpr)
        entry.insts.push(body);
    syms().exitScope();
    current_fn_          = previous_fn;
    current_fn_ret_type_ = previous_ret;
    active_builder_      = previous_builder;
    allowed_files_       = std::move(previous_allowed);
}

hir::HirExprId SemaPipeline::addHirExpr(hir::HirExpr expr, types::TypeId type) {
    auto id = hir_.addExpr(std::move(expr));
    while (id >= hir_types_.size())
        hir_types_.push(types::kErrorType);
    hir_types_[id] = type;
    return id;
}

types::TypeId SemaPipeline::exprType(hir::HirExprId id) const {
    if (id == hir::kInvalidHirExpr || id >= hir_types_.size())
        return types::kErrorType;
    return hir_types_[id];
}

bool SemaPipeline::run(const ast::ProgramNode &program) {
    // Pass 0: register user-defined types (structs, enums, unions, aliases)
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *sd = std::get_if<ast::StructDeclNode>(&decl))
            ctx_.types().registerNamedType(sd->name, types::TypeKind::Struct);
        else if (auto *ed = std::get_if<ast::EnumDeclNode>(&decl))
            ctx_.types().registerNamedType(ed->name, types::TypeKind::Enum);
        else if (auto *ud = std::get_if<ast::UnionDeclNode>(&decl))
            ctx_.types().registerNamedType(ud->name, types::TypeKind::Union);
        else if (auto *ad = std::get_if<ast::TypeAliasDeclNode>(&decl)) {
            types::TypeLower lower(ctx_.builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            auto target = lower.lower(ad->target_type);
            ctx_.types().registerTypeAlias(ad->name, target);
        }
    }

    // First pass: register stubs for functions declared in the main file only.
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *fn = std::get_if<ast::FnDeclNode>(&decl)) {
            for (symbols::SymId sym = 0; sym < ctx_.syms().symbolCount(); ++sym) {
                const auto &data = ctx_.syms().get(sym);
                if (data.kind == symbols::SymKind::Fn && data.decl_id == decl_id &&
                    (!import_mgr_ || !import_mgr_->originOf(sym))) {
                    ensureStub(sym);
                    break;
                }
            }
        }
    }

    // Populate allowed_files_ for the main file's own declarations.
    {
        memory::DynArray<size_t> saved = std::move(allowed_files_);
        if (import_mgr_) {
            for (auto dep : import_mgr_->rootDeps())
                allowed_files_.push(dep);
        }
        for (auto decl_id : program.decls) {
            auto &decl = ctx_.builder().getDecl(decl_id);
            if (auto *fn = std::get_if<ast::FnDeclNode>(&decl)) {
                for (symbols::SymId sym = 0; sym < ctx_.syms().symbolCount(); ++sym) {
                    const auto &data = ctx_.syms().get(sym);
                    if (data.kind == symbols::SymKind::Fn && data.decl_id == decl_id &&
                        (!import_mgr_ || !import_mgr_->originOf(sym))) {
                        ensureBodyLowered(sym);
                        break;
                    }
                }
            }
        }
        allowed_files_ = std::move(saved);
    }

    // Imported bodies are discovered lazily from calls made while lowering.
    for (size_t i = 0; i < worklist_.size(); ++i)
        ensureBodyLowered(worklist_[i]);

    ctx_.diags().emit();
    return !ctx_.diags().hasErrors();
}

hir::HirExprId SemaPipeline::visitExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return hir::kInvalidHirExpr;

    auto &node = builder().getExpr(id);
    return std::visit(
        [this, id](auto &n) -> hir::HirExprId {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, ast::LitValue>)
                return visitLiteral(n);
            if constexpr (std::is_same_v<T, ast::IdentNode>)
                return visitIdent(n, id);
            if constexpr (std::is_same_v<T, ast::BinaryNode>)
                return visitBinary(n);
            if constexpr (std::is_same_v<T, ast::UnaryNode>)
                return visitUnary(n);
            if constexpr (std::is_same_v<T, ast::CallNode>)
                return visitCall(n);
            if constexpr (std::is_same_v<T, ast::BlockNode>)
                return visitBlock(n);
            if constexpr (std::is_same_v<T, ast::IfNode>)
                return visitIf(n);
            if constexpr (std::is_same_v<T, ast::WhileNode>)
                return visitWhile(n);
            if constexpr (std::is_same_v<T, ast::FieldNode>)
                return hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::IndexNode>)
                return hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::RangeNode>)
                return hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::UnbodyNode>)
                return hir::kInvalidHirExpr;
            return hir::kInvalidHirExpr;
        },
        node);
}

hir::HirExprId SemaPipeline::visitLiteral(const ast::LitValue &n) {
    auto default_type = defaultTypeForLit(n.kind, ctx_.types());
    hir::HirLiteral lit{};
    lit.type = default_type;
    switch (n.kind) {
    case ast::LitKind::Int:
        lit.i = std::strtoll(std::string(n.raw).data(), nullptr, 10);
        break;
    case ast::LitKind::Float:
        lit.f = std::strtod(std::string(n.raw).data(), nullptr);
        break;
    case ast::LitKind::Bool:
        lit.b = (n.raw == "true");
        break;
    case ast::LitKind::Char: {
        auto raw = n.raw;
        lit.i    = raw.size() >= 3 && raw[0] == '\'' ? raw[1] : 0;
        break;
    }
    case ast::LitKind::String: {
        auto raw = n.raw;
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
            raw = raw.substr(1, raw.size() - 2);
        lit.str_val = ctx_.syms().interner().intern(raw);
        break;
    }
    default:
        break;
    }
    return addHirExpr(hir::HirExpr{lit}, default_type);
}

hir::HirExprId SemaPipeline::visitIdent(const ast::IdentNode &n, ast::ExprId id) {
    symbols::SymId sym = symbols::kInvalidSym;
    // Resolver entries belong exclusively to the main AstBuilder. Imported
    // builders reuse ExprId values, so using this table while lowering one of
    // them could resolve an unrelated main-file identifier.
    if (active_builder_ == &ctx_.builder() && resolved_ && id != ast::kInvalidExpr &&
        id < resolved_->size())
        sym = (*resolved_)[id];
    if (sym == symbols::kInvalidSym)
        sym = ctx_.syms().lookup(n.name);
    if (sym != symbols::kInvalidSym && !isSymAccessible(sym)) {
        ctx_.diags().report(diagnostics::Severity::Error, diagnostics::err::UndefinedIdent,
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
        return hir::kInvalidHirExpr;
    }

    auto &data = ctx_.syms().get(sym);

    hir::HirVar var;
    var.name    = ctx_.syms().interner().intern(n.name);
    var.version = 0;

    // Function names get unknown type so visitCall can do overload resolution
    types::TypeId ident_type = types::kErrorType;
    if (data.kind == symbols::SymKind::Fn)
        ident_type = ctx_.types().internUnknown();

    return addHirExpr(hir::HirExpr{var}, ident_type);
}

hir::HirExprId SemaPipeline::visitBinary(const ast::BinaryNode &n) {
    auto lhs = visitExpr(n.lhs);
    auto rhs = visitExpr(n.rhs);
    if (lhs == hir::kInvalidHirExpr || rhs == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    types::TypeId result_type = exprType(lhs);
    {
        types::TypeId rhs_type = exprType(rhs);
        if (result_type != types::kErrorType && rhs_type != types::kErrorType)
            unifier_.unify(result_type, rhs_type);
    }

    hir::HirBinary bin;
    bin.lhs = lhs;
    bin.rhs = rhs;
    bin.op  = mapBinaryOp(n.op);
    return addHirExpr(hir::HirExpr{bin}, result_type);
}

hir::HirExprId SemaPipeline::visitUnary(const ast::UnaryNode &n) {
    auto operand = visitExpr(n.operand);
    if (operand == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    auto operand_type = exprType(operand);

    hir::HirUnary un;
    un.op      = mapUnaryOp(n.op);
    un.operand = operand;
    return addHirExpr(hir::HirExpr{un}, operand_type);
}

hir::HirExprId SemaPipeline::visitCall(const ast::CallNode &n) {
    auto callee = visitExpr(n.callee);
    if (callee == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    auto callee_type = exprType(callee);

    memory::DynArray<hir::HirExprId> hir_args{hir_arena_};
    memory::DynArray<types::TypeId> arg_types{hir_arena_};
    for (auto arg : n.args) {
        auto hir_arg = visitExpr(arg);
        if (hir_arg != hir::kInvalidHirExpr) {
            hir_args.push(hir_arg);
            arg_types.push(exprType(hir_arg));
        }
    }

    symbols::SymId resolved_fn = symbols::kInvalidSym;
    types::TypeId result_type  = types::kErrorType;
    size_t match_count         = 0;

    // Overload resolution: if callee is an identifier (fn name)
    const auto &callee_expr = hir_.getExpr(callee);
    if (auto *var = std::get_if<hir::HirVar>(&callee_expr)) {
        auto callee_name          = var->name;
        auto candidates           = syms().lookupAll(callee_name, hir_arena_);
        size_t fn_candidate_count = 0;
        for (auto sym_id : candidates) {
            auto &data = syms().get(sym_id);
            if (data.kind != symbols::SymKind::Fn)
                continue;
            if (!isSymAccessible(sym_id))
                continue;
            fn_candidate_count++;
            if (data.decl_id == ast::kInvalidDecl)
                continue;

            ensureStub(sym_id);
            auto *bld = builderForSym(sym_id);
            if (!bld)
                continue;
            auto &decl    = bld->getDecl(data.decl_id);
            auto *fn_decl = std::get_if<ast::FnDeclNode>(&decl);
            if (!fn_decl)
                continue;

            // Arity check
            if (fn_decl->params.size() != n.args.size())
                continue;

            // Match against the stub associated with this symbol. DeclIds are
            // local to an AstBuilder and therefore are not globally unique.
            auto fi = hirIndexForSym(sym_id);
            if (fi != static_cast<size_t>(-1)) {
                auto &hfn = hir_.getFn(fi);

                bool match = true;
                for (size_t i = 0; i < hfn.params.size(); i++) {
                    if (hfn.params[i] == types::kErrorType)
                        continue;
                    if (i >= arg_types.size() || arg_types[i] == types::kErrorType)
                        continue;
                    if (!unifier_.isCoercible(hfn.params[i], arg_types[i])) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    match_count++;
                    if (match_count == 1) {
                        resolved_fn = sym_id;
                        result_type = hfn.return_type;
                        if (!fn_decl->is_extern && fn_decl->body != ast::kInvalidExpr)
                            worklist_.push(sym_id);
                    }
                }
            }
        }

        if (match_count == 0 && fn_candidate_count > 0) {
            if (n.args.size() > 0) {
                ctx_.diags().report(Severity::Error, NoMatchingFn,
                                    "no matching function for call to '" +
                                        std::string(syms().interner().lookup(callee_name)) + "'",
                                    n.span);
            } else {
                ctx_.diags().report(Severity::Error, WrongArity,
                                    "wrong number of arguments in call", n.span);
            }
        } else if (match_count > 1) {
            ctx_.diags().report(Severity::Error, AmbiguousCall,
                                "ambiguous call '" +
                                    std::string(syms().interner().lookup(callee_name)) +
                                    "' — multiple functions match",
                                n.span);
            resolved_fn = symbols::kInvalidSym;
            result_type = types::kErrorType;
        }
    }

    // Fallback: check callee as function type (fn pointers, etc.)
    if (resolved_fn == symbols::kInvalidSym && callee_type != types::kErrorType) {
        auto &fn_type = ctx_.types().lookup(callee_type);
        auto fn_ptr   = std::get_if<types::TypeFn>(&fn_type);
        if (fn_ptr) {
            if (hir_args.size() != fn_ptr->param_count) {
                ctx_.diags().report(Severity::Error, WrongArity,
                                    "wrong number of arguments in call", {});
            }
            for (size_t i = 0; i < hir_args.size() && i < fn_ptr->param_count; i++) {
                if (arg_types[i] != types::kErrorType)
                    unifier_.unify(arg_types[i], fn_ptr->params[i], n.span);
            }
            result_type = fn_ptr->ret;
        }
    }

    hir::HirCall call{callee, std::move(hir_args)};
    call.resolved_fn = resolved_fn;
    return addHirExpr(hir::HirExpr{std::move(call)}, result_type);
}

hir::HirExprId SemaPipeline::visitBlock(const ast::BlockNode &n) {
    // Process statements, collecting the last expression as trailing
    hir::HirExprId last = hir::kInvalidHirExpr;
    for (auto stmt_id : n.stmts) {
        visitStmt(stmt_id);
    }
    if (n.trailing != ast::kInvalidExpr) {
        last = visitExpr(n.trailing);
    }
    // Return the trailing expression with its type
    return last;
}

hir::HirExprId SemaPipeline::visitIf(const ast::IfNode &n) {
    auto cond                = visitExpr(n.cond);
    auto then_expr           = visitExpr(n.then_branch);
    hir::HirExprId else_expr = hir::kInvalidHirExpr;
    if (n.else_branch != ast::kInvalidExpr)
        else_expr = visitExpr(n.else_branch);

    types::TypeId result_type = types::kVoidType;
    if (then_expr != hir::kInvalidHirExpr) {
        result_type = exprType(then_expr);
        if (else_expr != hir::kInvalidHirExpr) {
            auto else_type = exprType(else_expr);
            if (result_type != types::kErrorType && else_type != types::kErrorType)
                unifier_.unify(result_type, else_type);
        }
    }

    hir::HirBranch branch;
    branch.cond = cond;
    // Both branches point to block 0 for now (skeleton)
    // Real block management will create proper then/else blocks
    branch.then_block = 0;
    branch.else_block = else_expr != hir::kInvalidHirExpr ? 0 : 0;
    return addHirExpr(hir::HirExpr{std::move(branch)}, result_type);
}

hir::HirExprId SemaPipeline::visitWhile(const ast::WhileNode &n) {
    auto cond = visitExpr(n.cond);
    auto body = visitExpr(n.body);
    // While loops don't produce a useful value in HIR yet
    return body;
}

void SemaPipeline::visitStmt(ast::StmtId id) {
    if (id == ast::kInvalidStmt)
        return;

    auto &node = builder().getStmt(id);

    struct StmtVisitor {
        SemaPipeline &pipeline;

        void operator()(const ast::LetNode &n) {
            auto sym_kind = symbols::SymKind::Variable;

            // Determine declared type from annotation or init
            types::TypeId decl_type = types::kErrorType;

            // Check type annotation if present
            if (n.type_annot != ast::kInvalidTypeExpr) {
                types::TypeLower lower(pipeline.builder(), pipeline.ctx_.types(),
                                       pipeline.ctx_.diags(), pipeline.ctx_.syms());
                decl_type = lower.lower(n.type_annot);
            }

            hir::HirExprId init     = hir::kInvalidHirExpr;
            types::TypeId init_type = types::kErrorType;
            if (n.init != ast::kInvalidExpr) {
                init = pipeline.visitExpr(n.init);
                if (init != hir::kInvalidHirExpr) {
                    init_type = pipeline.exprType(init);
                }
            }

            // If we have both annotation and init, unify them
            if (n.type_annot != ast::kInvalidTypeExpr && init != hir::kInvalidHirExpr) {
                if (decl_type != types::kErrorType && init_type != types::kErrorType)
                    pipeline.unifier_.unify(decl_type, init_type);
            }

            for (auto var_name : n.names) {
                pipeline.syms().declare(var_name, symbols::SymbolVisibility::Private, 0, sym_kind,
                                        ast::kInvalidDecl, memory::Span{});

                hir::HirLet hir_let;
                hir_let.name = pipeline.syms().interner().intern(var_name);
                hir_let.type = init_type;
                hir_let.init = init;

                auto hir_id = pipeline.addHirExpr(hir::HirExpr{hir_let}, init_type);
                if (pipeline.current_fn_) {
                    if (!pipeline.current_fn_->blocks.empty())
                        pipeline.current_fn_->blocks[0].insts.push(hir_id);
                }
            }
        }

        void operator()(const ast::AssignNode &n) {
            auto target = pipeline.visitExpr(n.target);
            auto value  = pipeline.visitExpr(n.value);
            (void)target;
            (void)value;
        }

        void operator()(const ast::RetNode &n) {
            hir::HirExprId val     = hir::kInvalidHirExpr;
            types::TypeId val_type = types::kVoidType;
            if (n.value != ast::kInvalidExpr) {
                val = pipeline.visitExpr(n.value);
                if (val != hir::kInvalidHirExpr)
                    val_type = pipeline.exprType(val);
            }

            // Check return type compatibility (skip if either is error sentinel)
            if (val_type != types::kErrorType && pipeline.current_fn_ret_type_ != types::kErrorType)
                pipeline.unifier_.unify(val_type, pipeline.current_fn_ret_type_);

            hir::HirRet ret;
            ret.value   = val;
            auto hir_id = pipeline.hir_.addExpr(hir::HirExpr{ret});
            if (pipeline.current_fn_ && !pipeline.current_fn_->blocks.empty())
                pipeline.current_fn_->blocks[0].terminator = hir_id;
        }

        void operator()(ast::ExprId expr_id) {
            auto result = pipeline.visitExpr(expr_id);
            if (result != hir::kInvalidHirExpr && pipeline.current_fn_ &&
                !pipeline.current_fn_->blocks.empty()) {
                pipeline.current_fn_->blocks[0].insts.push(result);
            }
        }
    };

    std::visit(StmtVisitor{*this}, node);
}

void SemaPipeline::visitDecl(const ast::FnDeclNode &n) {
    // Already handled in run() — this is a placeholder for future use
    (void)n;
}

types::TypeId SemaPipeline::inferExprType(ast::ExprId id) {
    auto hir_id = visitExpr(id);
    if (hir_id == hir::kInvalidHirExpr)
        return types::kErrorType;
    return exprType(hir_id);
}

} // namespace zith::sema
