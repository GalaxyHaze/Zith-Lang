#include "sema/hir-lower.hpp"
#include "ast/ast-node-utils.hpp"
#include "diagnostics/error-codes.hpp"
#include "symbols/symbol-id.hpp"
#include "types/type-id.hpp"
#include "types/type-lower.hpp"

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

} // namespace

HirLower::HirLower(symbols::SymbolTable &syms, types::TypeIntern &types,
                   diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                   memory::Arena &hir_arena, const TypedAst &typed_ast,
                   const memory::DynArray<symbols::SymId> *resolved,
                   symbols::ImportManager *import_mgr)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_arena_(hir_arena),
      hir_(hir_arena), typed_ast_(typed_ast), resolved_(resolved), import_mgr_(import_mgr),
      worklist_(hir_arena), fn_syms_(hir_arena), active_builder_(&builder),
      allowed_files_(hir_arena), var_types_(hir_arena) {}

ast::AstBuilder &HirLower::builder() const {
    return *active_builder_;
}

ast::AstBuilder *HirLower::builderForSym(symbols::SymId fn_sym) {
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            return import_mgr_->get(origin->file_idx).builder;
    }
    return &ctx_.builder();
}

const symbols::SymbolTable *HirLower::symsForSym(symbols::SymId fn_sym) {
    if (import_mgr_) {
        auto origin = import_mgr_->originOf(fn_sym);
        if (origin)
            return &import_mgr_->get(origin->file_idx).symbols;
    }
    return &ctx_.syms();
}

size_t HirLower::hirIndexForSym(symbols::SymId fn_sym) const {
    for (size_t i = 0; i < fn_syms_.size(); ++i)
        if (fn_syms_[i] == fn_sym)
            return i;
    return static_cast<size_t>(-1);
}

bool HirLower::isSymAccessible(symbols::SymId sym_id) const {
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

types::TypeId HirLower::astExprType(ast::ExprId id) const {
    return typed_ast_.get(id);
}

hir::HirExprId HirLower::addHirExpr(hir::HirExpr expr) {
    return hir_.addExpr(std::move(expr));
}

void HirLower::ensureStub(symbols::SymId fn_sym) {
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
    auto *fn = ast::asFn(source_bld->getDecl(data.decl_id));
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

void HirLower::ensureBodyLowered(symbols::SymId fn_sym) {
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
    auto *fn         = ast::asFn(source_bld->getDecl(data.decl_id));
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
    auto scope             = ctx_.syms().enterScope();
    ctx_.syms().emplace(*source_syms, scope);
    for (const auto &param : fn->params)
        ctx_.syms().declareInScope(scope, param.name);
    hfn.blocks.emplace(hir_arena_);
    currentBlock_ = 0;
    labelMap_.clear();
    auto &entry = hfn.blocks[0];
    entry.insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    auto body   = visitExpr(fn->body);
    if (body != hir::kInvalidHirExpr && entry.terminator == hir::kInvalidHirExpr) {
        if (current_fn_ret_type_ != types::kErrorType) {
            hir::HirRet ret;
            ret.value = body;
            setTerminator(hir_.addExpr(hir::HirExpr{ret}));
        } else {
            entry.insts.push(body);
        }
    }
    ctx_.syms().exitScope();
    current_fn_          = previous_fn;
    current_fn_ret_type_ = previous_ret;
    active_builder_      = previous_builder;
    allowed_files_       = std::move(previous_allowed);
}

bool HirLower::run(const ast::ProgramNode &program) {
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (ast::asFn(decl)) {
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

    {
        memory::DynArray<size_t> saved = std::move(allowed_files_);
        if (import_mgr_) {
            for (auto dep : import_mgr_->rootDeps())
                allowed_files_.push(dep);
        }
        for (auto decl_id : program.decls) {
            auto &decl = ctx_.builder().getDecl(decl_id);
            if (ast::asFn(decl)) {
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

    for (size_t i = 0; i < worklist_.size(); ++i)
        ensureBodyLowered(worklist_[i]);

    return !ctx_.diags().hasErrors();
}

hir::HirExprId HirLower::visitExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return hir::kInvalidHirExpr;

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
        return visitBlock(std::get<ast::BlockNode>(node));
    case ast::ExprKind::If:
        return visitIf(std::get<ast::IfNode>(node));
    case ast::ExprKind::While:
        return visitWhile(std::get<ast::WhileNode>(node));
    case ast::ExprKind::Field:
    case ast::ExprKind::Index:
    case ast::ExprKind::Range:
    case ast::ExprKind::Unbody:
    case ast::ExprKind::Intrinsic:
    case ast::ExprKind::MacroCall:
        return hir::kInvalidHirExpr;
    }
    return hir::kInvalidHirExpr;
}

hir::HirExprId HirLower::visitLiteral(ast::ExprId id, const ast::LitValue &n) {
    auto literal_type = astExprType(id);
    if (literal_type == types::kErrorType)
        literal_type = defaultTypeForLit(n.kind, ctx_.types());

    hir::HirLiteral lit{};
    lit.type = literal_type;
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
    return addHirExpr(hir::HirExpr{lit});
}

hir::HirExprId HirLower::visitIdent(const ast::IdentNode &n, ast::ExprId id) {
    symbols::SymId sym = symbols::kInvalidSym;
    if (active_builder_ == &ctx_.builder() && resolved_ && id != ast::kInvalidExpr &&
        id < resolved_->size())
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
        return hir::kInvalidHirExpr;
    }

    hir::HirVar var;
    var.name    = ctx_.syms().interner().intern(n.name);
    var.version = 0;
    return addHirExpr(hir::HirExpr{var});
}

hir::HirExprId HirLower::visitBinary(ast::ExprId id, const ast::BinaryNode &n) {
    auto lhs = visitExpr(n.lhs);
    auto rhs = visitExpr(n.rhs);
    if (lhs == hir::kInvalidHirExpr || rhs == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    hir::HirBinary bin;
    bin.lhs = lhs;
    bin.rhs = rhs;
    bin.op  = mapBinaryOp(n.op);
    (void)id;
    return addHirExpr(hir::HirExpr{bin});
}

hir::HirExprId HirLower::visitUnary(ast::ExprId, const ast::UnaryNode &n) {
    auto operand = visitExpr(n.operand);
    if (operand == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    hir::HirUnary un;
    un.op      = mapUnaryOp(n.op);
    un.operand = operand;
    return addHirExpr(hir::HirExpr{un});
}

hir::HirExprId HirLower::visitCall(ast::ExprId id, const ast::CallNode &n) {
    auto callee = visitExpr(n.callee);
    if (callee == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    auto callee_type = astExprType(n.callee);

    memory::DynArray<hir::HirExprId> hir_args{hir_arena_};
    memory::DynArray<types::TypeId> arg_types{hir_arena_};
    for (auto arg : n.args) {
        auto hir_arg = visitExpr(arg);
        if (hir_arg != hir::kInvalidHirExpr) {
            hir_args.push(hir_arg);
            arg_types.push(astExprType(arg));
        }
    }

    symbols::SymId resolved_fn = symbols::kInvalidSym;
    size_t match_count         = 0;

    const auto &callee_expr = hir_.getExpr(callee);
    if (hir::exprKind(callee_expr) == hir::HirExprKind::Var) {
        auto &var                 = std::get<hir::HirVar>(callee_expr);
        auto callee_name          = var.name;
        auto candidates           = ctx_.syms().lookupAll(callee_name, hir_arena_);
        size_t fn_candidate_count = 0;
        for (auto sym_id : candidates) {
            auto &data = ctx_.syms().get(sym_id);
            if (data.kind != symbols::SymKind::Fn)
                continue;
            if (!isSymAccessible(sym_id))
                continue;
            fn_candidate_count++;
            if (data.decl_id == ast::kInvalidDecl)
                continue;

            ensureStub(sym_id);
            auto fi = hirIndexForSym(sym_id);
            if (fi == static_cast<size_t>(-1))
                continue;

            auto &hfn = hir_.getFn(fi);
            if (hfn.params.size() != n.args.size())
                continue;

            bool match = true;
            for (size_t i = 0; i < hfn.params.size(); i++) {
                if (i >= arg_types.size() || arg_types[i] == types::kErrorType)
                    continue;
                if (hfn.params[i] == types::kErrorType)
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
                    auto *bld   = builderForSym(sym_id);
                    if (bld) {
                        auto local_sym = sym_id;
                        if (import_mgr_) {
                            auto origin = import_mgr_->originOf(sym_id);
                            if (origin)
                                local_sym = origin->local_sym;
                        }
                        auto *source_syms = symsForSym(sym_id);
                        if (source_syms) {
                            const auto &source_data = source_syms->get(local_sym);
                            auto *fn_decl           = ast::asFn(bld->getDecl(source_data.decl_id));
                            if (fn_decl && !fn_decl->is_extern &&
                                fn_decl->body != ast::kInvalidExpr)
                                worklist_.push(sym_id);
                        }
                    }
                }
            }
        }

        if (match_count == 0 && fn_candidate_count > 0) {
            if (!n.args.empty()) {
                ctx_.diags().report(Severity::Error, NoMatchingFn,
                                    "no matching function for call to '" +
                                        std::string(ctx_.syms().interner().lookup(callee_name)) +
                                        "'",
                                    n.span);
            } else {
                ctx_.diags().report(Severity::Error, WrongArity,
                                    "wrong number of arguments in call", n.span);
            }
        } else if (match_count > 1) {
            ctx_.diags().report(Severity::Error, AmbiguousCall,
                                "ambiguous call '" +
                                    std::string(ctx_.syms().interner().lookup(callee_name)) +
                                    "' — multiple functions match",
                                n.span);
            resolved_fn = symbols::kInvalidSym;
        }
    }

    if (resolved_fn == symbols::kInvalidSym && callee_type != types::kErrorType) {
        auto &fn_type = ctx_.types().lookup(callee_type);
        auto fn_ptr   = std::get_if<types::TypeFn>(&fn_type);
        if (fn_ptr && hir_args.size() != fn_ptr->param_count) {
            ctx_.diags().report(Severity::Error, WrongArity, "wrong number of arguments in call",
                                {});
        }
    }

    hir::HirCall call{callee, std::move(hir_args)};
    call.resolved_fn = resolved_fn;
    (void)id;
    return addHirExpr(hir::HirExpr{std::move(call)});
}

hir::HirExprId HirLower::visitBlock(const ast::BlockNode &n) {
    hir::HirExprId last = hir::kInvalidHirExpr;
    for (auto stmt_id : n.stmts)
        visitStmt(stmt_id);
    if (n.trailing != ast::kInvalidExpr)
        last = visitExpr(n.trailing);
    return last;
}

hir::HirExprId HirLower::visitIf(const ast::IfNode &n) {
    auto origBlock = currentBlock_;
    auto cond      = visitExpr(n.cond);

    auto then_block = newBlock();
    setCurrentBlock(then_block);
    current_fn_->blocks[then_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    visitExpr(n.then_branch);
    bool thenTerminated = current_fn_->blocks[then_block].terminator != hir::kInvalidHirExpr;

    size_t else_target;

    if (n.else_branch != ast::kInvalidExpr) {
        auto else_block = newBlock();
        setCurrentBlock(else_block);
        current_fn_->blocks[else_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
        visitExpr(n.else_branch);
        bool elseTerminated = current_fn_->blocks[else_block].terminator != hir::kInvalidHirExpr;

        if (thenTerminated && elseTerminated) {
            size_t dead = newBlock();
            setCurrentBlock(dead);
            current_fn_->blocks[dead].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                     = else_block;
        } else if (thenTerminated) {
            else_target = else_block;
        } else if (elseTerminated) {
            auto merge = newBlock();
            setCurrentBlock(then_block);
            emitJump(merge);
            setCurrentBlock(merge);
            current_fn_->blocks[merge].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                      = else_block;
        } else {
            auto merge = newBlock();
            setCurrentBlock(then_block);
            emitJump(merge);
            setCurrentBlock(else_block);
            emitJump(merge);
            setCurrentBlock(merge);
            current_fn_->blocks[merge].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                      = else_block;
        }
    } else {
        if (thenTerminated) {
            auto cont = newBlock();
            setCurrentBlock(cont);
            current_fn_->blocks[cont].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                     = cont;
        } else {
            auto cont = newBlock();
            setCurrentBlock(then_block);
            emitJump(cont);
            setCurrentBlock(cont);
            current_fn_->blocks[cont].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                     = cont;
        }
    }

    hir::HirBranch branch;
    branch.cond                               = cond;
    branch.then_block                         = static_cast<hir::HirDeclId>(then_block);
    branch.else_block                         = static_cast<hir::HirDeclId>(else_target);
    current_fn_->blocks[origBlock].terminator = hir_.addExpr(hir::HirExpr{std::move(branch)});
    return hir::kInvalidHirExpr;
}

hir::HirExprId HirLower::visitWhile(const ast::WhileNode &n) {
    auto header_block = newBlock();
    auto body_block   = newBlock();
    auto exit_block   = newBlock();

    emitJump(header_block);

    setCurrentBlock(header_block);
    current_fn_->blocks[header_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    auto cond                               = visitExpr(n.cond);

    hir::HirBranch branch;
    branch.cond       = cond;
    branch.then_block = static_cast<hir::HirDeclId>(body_block);
    branch.else_block = static_cast<hir::HirDeclId>(exit_block);
    setTerminator(hir_.addExpr(hir::HirExpr{std::move(branch)}));

    setCurrentBlock(body_block);
    current_fn_->blocks[body_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    visitExpr(n.body);
    if (current_fn_->blocks[body_block].terminator == hir::kInvalidHirExpr)
        emitJump(header_block);

    setCurrentBlock(exit_block);
    current_fn_->blocks[exit_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    return hir::kInvalidHirExpr;
}

size_t HirLower::newBlock() {
    auto idx = current_fn_->blocks.size();
    current_fn_->blocks.emplace(hir_arena_);
    return idx;
}

void HirLower::setCurrentBlock(size_t idx) {
    currentBlock_ = idx;
}

size_t HirLower::currentBlock() const {
    return currentBlock_;
}

void HirLower::setTerminator(hir::HirExprId term) {
    current_fn_->blocks[currentBlock_].terminator = term;
}

void HirLower::emitJump(size_t target) {
    hir::HirJump jump;
    jump.target = static_cast<hir::HirDeclId>(target);
    setTerminator(hir_.addExpr(hir::HirExpr{std::move(jump)}));
}

void HirLower::visitMarker(const ast::MarkerNode &n) {
    auto prevBlock   = currentBlock_;
    auto markerBlock = newBlock();
    auto postBlock   = newBlock();

    labelMap_.insert(n.name, markerBlock);

    hir::HirJump jump;
    jump.target                               = static_cast<hir::HirDeclId>(postBlock);
    current_fn_->blocks[prevBlock].terminator = hir_.addExpr(hir::HirExpr{std::move(jump)});

    setCurrentBlock(markerBlock);
    current_fn_->blocks[markerBlock].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    for (auto stmt_id : n.body)
        visitStmt(stmt_id);

    if (current_fn_->blocks[markerBlock].terminator == hir::kInvalidHirExpr) {
        hir::HirJump endJump;
        endJump.target = static_cast<hir::HirDeclId>(postBlock);
        current_fn_->blocks[markerBlock].terminator =
            hir_.addExpr(hir::HirExpr{std::move(endJump)});
    }

    setCurrentBlock(postBlock);
    current_fn_->blocks[postBlock].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
}

void HirLower::visitGoto(const ast::GotoNode &n) {
    auto *targetIdx = labelMap_.get(n.target);
    if (!targetIdx) {
        ctx_.diags().report(Severity::Error, UndefinedIdent,
                            "undefined marker '" + std::string(n.target) + "'", n.span);
        return;
    }
    emitJump(*targetIdx);
    auto dead = newBlock();
    setCurrentBlock(dead);
    current_fn_->blocks[dead].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
}

void HirLower::visitStmt(ast::StmtId id) {
    if (id == ast::kInvalidStmt)
        return;

    auto &node = builder().getStmt(id);
    switch (ast::stmtKind(node)) {
    case ast::StmtKind::Let: {
        const auto &n           = std::get<ast::LetNode>(node);
        hir::HirExprId init     = hir::kInvalidHirExpr;
        types::TypeId init_type = types::kErrorType;
        if (n.init != ast::kInvalidExpr) {
            init = visitExpr(n.init);
            if (init != hir::kInvalidHirExpr)
                init_type = astExprType(n.init);
        }

        types::TypeId decl_type = types::kErrorType;
        if (n.type_annot != ast::kInvalidTypeExpr) {
            types::TypeLower lower(builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            decl_type = lower.lower(n.type_annot);
        }

        types::TypeId var_type = init_type;
        if (n.type_annot != ast::kInvalidTypeExpr && decl_type != types::kErrorType)
            var_type = decl_type;
        else if (init_type != types::kErrorType)
            var_type = init_type;

        for (auto var_name : n.names) {
            auto sym_id =
                ctx_.syms().declare(var_name, symbols::SymbolVisibility::Private, 0,
                                    symbols::SymKind::Variable, ast::kInvalidDecl, memory::Span{});
            if (sym_id != symbols::kInvalidSym) {
                if (sym_id >= var_types_.size()) {
                    if (sym_id >= var_types_.capacity())
                        var_types_.reserve(sym_id + 1);
                    while (var_types_.size() <= sym_id)
                        var_types_.push(types::kErrorType);
                }
                var_types_[sym_id] = var_type;
            }

            hir::HirLet hir_let;
            hir_let.name = ctx_.syms().interner().intern(var_name);
            hir_let.type = var_type;
            hir_let.init = init;

            auto hir_id = addHirExpr(hir::HirExpr{hir_let});
            if (current_fn_ && !current_fn_->blocks.empty()) {
                auto &cur = current_fn_->blocks[currentBlock()];
                cur.insts.push(hir_id);
            }
        }
        break;
    }
    case ast::StmtKind::Assign: {
        const auto &n = std::get<ast::AssignNode>(node);
        auto target   = visitExpr(n.target);
        auto value    = visitExpr(n.value);
        (void)target;
        (void)value;
        break;
    }
    case ast::StmtKind::Return: {
        const auto &n      = std::get<ast::RetNode>(node);
        hir::HirExprId val = hir::kInvalidHirExpr;
        if (n.value != ast::kInvalidExpr)
            val = visitExpr(n.value);

        hir::HirRet ret;
        ret.value   = val;
        auto hir_id = hir_.addExpr(hir::HirExpr{ret});
        if (current_fn_ && !current_fn_->blocks.empty())
            setTerminator(hir_id);
        break;
    }
    case ast::StmtKind::Goto:
        visitGoto(std::get<ast::GotoNode>(node));
        break;
    case ast::StmtKind::Marker:
        visitMarker(std::get<ast::MarkerNode>(node));
        break;
    case ast::StmtKind::Expr: {
        auto expr_id = std::get<ast::ExprStmtNode>(node).expr;
        auto result  = visitExpr(expr_id);
        if (result != hir::kInvalidHirExpr && current_fn_ && !current_fn_->blocks.empty()) {
            auto &cur = current_fn_->blocks[currentBlock()];
            cur.insts.push(result);
        }
        break;
    }
    }
}

} // namespace zith::sema
