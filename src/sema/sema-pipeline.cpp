#include "sema-pipeline.hpp"
#include "ast/ast-node-utils.hpp"
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

/// Return the bit width of an IntWidth (0 for Literal).
static unsigned intWidthBits(types::IntWidth w) {
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

/// True if the IntWidth is a signed type (I8..I128).
static bool isSignedWidth(types::IntWidth w) {
    return w >= types::IntWidth::I8 && w <= types::IntWidth::I128;
}

/// Check whether int64_t value fits in an integer of the given concrete width.
/// Returns nullptr on success, or an error message on overflow.
static const char *checkIntOverflow(int64_t value, types::IntWidth target) {
    unsigned bits = intWidthBits(target);
    if (bits == 0)
        return nullptr; // Literal — unresolved
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

/// If a HIR expression is a HirLiteral with a Literal (unresolved) integer width,
/// set its type to the given concrete TypeId and return true.
static bool resolveLiteralType(hir::HirModule &hir, hir::HirExprId id, types::TypeIntern &types,
                               types::TypeId concreteType) {
    auto &expr = hir.getExprMut(id);
    if (hir::exprKind(expr) != hir::HirExprKind::Literal)
        return false;
    auto &lit = std::get<hir::HirLiteral>(expr);
    auto kind = types.kindOf(lit.type);
    if (kind != types::TypeKind::Int)
        return false;
    auto &int_t = std::get<types::TypeInt>(types.lookup(lit.type));
    if (int_t.width != types::IntWidth::Literal)
        return false;
    lit.type = concreteType;
    return true;
}

} // anonymous namespace

SemaPipeline::SemaPipeline(symbols::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &hir_arena,
                           const memory::DynArray<symbols::SymId> *resolved,
                           symbols::ImportManager *import_mgr)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_arena_(hir_arena),
      hir_(hir_arena), resolved_(resolved), hir_types_(hir_arena), import_mgr_(import_mgr),
      worklist_(hir_arena), fn_syms_(hir_arena), active_builder_(&builder),
      allowed_files_(hir_arena), var_types_(hir_arena) {}

bool SemaPipeline::isSymAccessible(symbols::SymId sym_id) const {
    if (allowed_files_.empty())
        return true;
    auto origin = import_mgr_ ? import_mgr_->originOf(sym_id)
                              : memory::Optional<symbols::SymOrigin>{};
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
    auto scope             = syms().enterScope();
    syms().emplace(*source_syms, scope);
    for (const auto &param : fn->params)
        syms().declareInScope(scope, param.name);
    hfn.blocks.emplace(hir_arena_);
    currentBlock_ = 0;
    labelMap_.clear();
    auto &entry = hfn.blocks[0];
    entry.insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    auto body   = visitExpr(fn->body);
    // If the function body has a trailing expression, no explicit return was
    // created during lowering, and the function has a declared return type,
    // turn the body value into a HirRet after unifying types.
    if (body != hir::kInvalidHirExpr && entry.terminator == hir::kInvalidHirExpr) {
        if (current_fn_ret_type_ != types::kErrorType) {
            auto body_type = exprType(body);
            if (body_type != types::kErrorType &&
                unifier_.unify(body_type, current_fn_ret_type_, fn->span)) {
                // Resolve literal type if needed and check overflow
                types::TypeId concrete = current_fn_ret_type_;
                {
                    auto k = ctx_.types().kindOf(current_fn_ret_type_);
                    if (k == types::TypeKind::Int) {
                        auto &t =
                            std::get<types::TypeInt>(ctx_.types().lookup(current_fn_ret_type_));
                        if (t.width == types::IntWidth::Literal)
                            concrete = body_type;
                    }
                }
                if (resolveLiteralType(hir_, body, ctx_.types(), concrete)) {
                    auto &expr  = hir_.getExpr(body);
                    auto &lit   = std::get<hir::HirLiteral>(expr);
                    auto &int_t = std::get<types::TypeInt>(ctx_.types().lookup(concrete));
                    if (auto *err = checkIntOverflow(lit.i, int_t.width)) {
                        ctx_.diags().report(Severity::Error, TypeMismatch, err, fn->span);
                    }
                }
            }
            hir::HirRet ret;
            ret.value = body;
            setTerminator(hir_.addExpr(hir::HirExpr{ret}));
        } else {
            // No return type annotation — push trailing expression as a
            // regular instruction so its value is evaluated but discarded.
            entry.insts.push(body);
        }
    }
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

    // First pass: register stubs for functions declared in the main file only.
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

    // Populate allowed_files_ for the main file's own declarations.
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
    switch (ast::exprKind(node)) {
    case ast::ExprKind::Literal:
        return visitLiteral(std::get<ast::LitValue>(node));
    case ast::ExprKind::Identifier:
        return visitIdent(std::get<ast::IdentNode>(node), id);
    case ast::ExprKind::Binary:
        return visitBinary(std::get<ast::BinaryNode>(node));
    case ast::ExprKind::Unary:
        return visitUnary(std::get<ast::UnaryNode>(node));
    case ast::ExprKind::Call:
        return visitCall(std::get<ast::CallNode>(node));
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

    // Determine the expression type from the symbol
    types::TypeId ident_type = types::kErrorType;
    if (data.kind == symbols::SymKind::Fn) {
        ident_type = ctx_.types().internUnknown();
    } else if (data.kind == symbols::SymKind::Variable && sym < var_types_.size()) {
        ident_type = var_types_[sym];
    }

    return addHirExpr(hir::HirExpr{var}, ident_type);
}

hir::HirExprId SemaPipeline::visitBinary(const ast::BinaryNode &n) {
    auto lhs = visitExpr(n.lhs);
    auto rhs = visitExpr(n.rhs);
    if (lhs == hir::kInvalidHirExpr || rhs == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    types::TypeId result_type = exprType(lhs);
    types::TypeId rhs_type    = exprType(rhs);
    if (result_type != types::kErrorType && rhs_type != types::kErrorType) {
        if (unifier_.unify(result_type, rhs_type, n.span)) {
            // Determine the concrete type after unification: the non-Literal
            // side gives us the concrete width, or fall back to result_type.
            types::TypeId concrete = result_type;
            {
                auto k = ctx_.types().kindOf(result_type);
                if (k == types::TypeKind::Int) {
                    auto &t = std::get<types::TypeInt>(ctx_.types().lookup(result_type));
                    if (t.width == types::IntWidth::Literal)
                        concrete = rhs_type;
                }
            }
            // Propagate concrete type to literal operands and check overflow
            for (auto id : {lhs, rhs}) {
                if (resolveLiteralType(hir_, id, ctx_.types(), concrete)) {
                    auto &expr  = hir_.getExpr(id);
                    auto &lit   = std::get<hir::HirLiteral>(expr);
                    auto &int_t = std::get<types::TypeInt>(ctx_.types().lookup(concrete));
                    if (auto *err = checkIntOverflow(lit.i, int_t.width)) {
                        ctx_.diags().report(Severity::Error, TypeMismatch, err, n.span);
                        return hir::kInvalidHirExpr;
                    }
                }
            }
        }
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
    if (hir::exprKind(callee_expr) == hir::HirExprKind::Var) {
        auto &var                 = std::get<hir::HirVar>(callee_expr);
        auto callee_name          = var.name;
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
            auto *fn_decl = ast::asFn(decl);
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

    if (resolved_fn != symbols::kInvalidSym) {
        auto fi = hirIndexForSym(resolved_fn);
        if (fi != static_cast<size_t>(-1)) {
            auto &hfn = hir_.getFn(fi);
            for (size_t i = 0; i < hir_args.size() && i < hfn.params.size(); i++) {
                if (arg_types[i] != types::kErrorType && hfn.params[i] != types::kErrorType)
                    resolveLiteralType(hir_, hir_args[i], ctx_.types(), hfn.params[i]);
            }
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
    auto origBlock = currentBlock_;
    auto cond      = visitExpr(n.cond);

    auto then_block = newBlock();
    setCurrentBlock(then_block);
    current_fn_->blocks[then_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);

    // Visit then branch — may add a terminator (jump/return)
    visitExpr(n.then_branch);
    bool thenTerminated = current_fn_->blocks[then_block].terminator != hir::kInvalidHirExpr;

    size_t else_target; // what the branch's else field points to

    if (n.else_branch != ast::kInvalidExpr) {
        auto else_block = newBlock();
        setCurrentBlock(else_block);
        current_fn_->blocks[else_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
        visitExpr(n.else_branch);
        bool elseTerminated = current_fn_->blocks[else_block].terminator != hir::kInvalidHirExpr;

        if (thenTerminated && elseTerminated) {
            // Both terminated — no merge needed
            size_t dead = newBlock();
            setCurrentBlock(dead);
            current_fn_->blocks[dead].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                     = else_block;
        } else if (thenTerminated) {
            // Then terminated, else didn't — else needs to continue
            else_target = else_block;
            // current block stays in else_block, it continues naturally
        } else if (elseTerminated) {
            // Else terminated, then didn't — then jumps to else's continuation
            auto merge = newBlock();
            setCurrentBlock(then_block);
            emitJump(merge);
            setCurrentBlock(merge);
            current_fn_->blocks[merge].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
            else_target                      = else_block;
        } else {
            // Neither terminated — both need to merge
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
        // No else — branch on false goes to continuation
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

    // Set branch terminator on original block
    hir::HirBranch branch;
    branch.cond                               = cond;
    branch.then_block                         = static_cast<hir::HirDeclId>(then_block);
    branch.else_block                         = static_cast<hir::HirDeclId>(else_target);
    current_fn_->blocks[origBlock].terminator = hir_.addExpr(hir::HirExpr{std::move(branch)});

    return hir::kInvalidHirExpr;
}

hir::HirExprId SemaPipeline::visitWhile(const ast::WhileNode &n) {
    auto header_block = newBlock();
    auto body_block   = newBlock();
    auto exit_block   = newBlock();

    // Jump from current block to header
    emitJump(header_block);

    // Emit header block
    setCurrentBlock(header_block);
    current_fn_->blocks[header_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    auto cond                               = visitExpr(n.cond);

    hir::HirBranch branch;
    branch.cond       = cond;
    branch.then_block = static_cast<hir::HirDeclId>(body_block);
    branch.else_block = static_cast<hir::HirDeclId>(exit_block);
    setTerminator(hir_.addExpr(hir::HirExpr{std::move(branch)}));

    // Emit body block
    setCurrentBlock(body_block);
    current_fn_->blocks[body_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    visitExpr(n.body);
    // Jump back to header after body
    if (current_fn_->blocks[body_block].terminator == hir::kInvalidHirExpr)
        emitJump(header_block);

    // Switch to exit block
    setCurrentBlock(exit_block);
    current_fn_->blocks[exit_block].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    return hir::kInvalidHirExpr;
}

// ── block management ──

size_t SemaPipeline::newBlock() {
    auto idx = current_fn_->blocks.size();
    current_fn_->blocks.emplace(hir_arena_);
    return idx;
}

void SemaPipeline::setTerminator(hir::HirExprId term) {
    current_fn_->blocks[currentBlock_].terminator = term;
}

void SemaPipeline::emitJump(size_t target) {
    hir::HirJump jump;
    jump.target = static_cast<hir::HirDeclId>(target);
    setTerminator(hir_.addExpr(hir::HirExpr{std::move(jump)}));
}

void SemaPipeline::visitMarker(const ast::MarkerNode &n) {
    auto prevBlock   = currentBlock_;
    auto markerBlock = newBlock();
    auto postBlock   = newBlock();

    labelMap_.insert(n.name, markerBlock);

    // Jump from prevBlock to postBlock (skip marker body during normal flow)
    hir::HirJump jump;
    jump.target                               = static_cast<hir::HirDeclId>(postBlock);
    current_fn_->blocks[prevBlock].terminator = hir_.addExpr(hir::HirExpr{std::move(jump)});

    // Emit marker body
    setCurrentBlock(markerBlock);
    current_fn_->blocks[markerBlock].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
    for (auto stmt_id : n.body)
        visitStmt(stmt_id);

    // If marker body didn't terminate, jump to postBlock
    if (current_fn_->blocks[markerBlock].terminator == hir::kInvalidHirExpr) {
        hir::HirJump endJump;
        endJump.target = static_cast<hir::HirDeclId>(postBlock);
        current_fn_->blocks[markerBlock].terminator =
            hir_.addExpr(hir::HirExpr{std::move(endJump)});
    }

    // Continue in postBlock
    setCurrentBlock(postBlock);
    current_fn_->blocks[postBlock].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
}

void SemaPipeline::visitGoto(const ast::GotoNode &n) {
    auto *targetIdx = labelMap_.get(n.target);
    if (!targetIdx) {
        ctx_.diags().report(Severity::Error, diagnostics::err::UndefinedIdent,
                            "undefined marker '" + std::string(n.target) + "'", n.span);
        return;
    }
    emitJump(*targetIdx);
    // Subsequent code in this block is unreachable, create a new block
    auto dead = newBlock();
    setCurrentBlock(dead);
    current_fn_->blocks[dead].insts = memory::DynArray<hir::HirExprId>(hir_arena_);
}

void SemaPipeline::visitStmt(ast::StmtId id) {
    if (id == ast::kInvalidStmt)
        return;

    auto &node = builder().getStmt(id);
    switch (ast::stmtKind(node)) {
    case ast::StmtKind::Let: {
        const auto &n = std::get<ast::LetNode>(node);
        auto sym_kind = symbols::SymKind::Variable;

        types::TypeId decl_type = types::kErrorType;
        if (n.type_annot != ast::kInvalidTypeExpr) {
            types::TypeLower lower(builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            decl_type = lower.lower(n.type_annot);
        }

        hir::HirExprId init     = hir::kInvalidHirExpr;
        types::TypeId init_type = types::kErrorType;
        if (n.init != ast::kInvalidExpr) {
            init = visitExpr(n.init);
            if (init != hir::kInvalidHirExpr)
                init_type = exprType(init);
        }

        if (n.type_annot != ast::kInvalidTypeExpr && init != hir::kInvalidHirExpr) {
            if (decl_type != types::kErrorType && init_type != types::kErrorType) {
                if (unifier_.unify(decl_type, init_type, n.span)) {
                    types::TypeId concrete = decl_type;
                    auto kind              = ctx_.types().kindOf(decl_type);
                    if (kind == types::TypeKind::Int) {
                        auto &type = std::get<types::TypeInt>(ctx_.types().lookup(decl_type));
                        if (type.width == types::IntWidth::Literal)
                            concrete = init_type;
                    }
                    if (resolveLiteralType(hir_, init, ctx_.types(), concrete)) {
                        auto &expr  = hir_.getExpr(init);
                        auto &lit   = std::get<hir::HirLiteral>(expr);
                        auto &int_t = std::get<types::TypeInt>(ctx_.types().lookup(concrete));
                        if (auto *err = checkIntOverflow(lit.i, int_t.width))
                            ctx_.diags().report(Severity::Error, TypeMismatch, err, n.span);
                    }
                }
            }
        }

        types::TypeId var_type = init_type;
        if (n.type_annot != ast::kInvalidTypeExpr && decl_type != types::kErrorType)
            var_type = decl_type;
        else if (init_type != types::kErrorType)
            var_type = init_type;

        for (auto var_name : n.names) {
            auto sym_id = syms().declare(var_name, symbols::SymbolVisibility::Private, 0, sym_kind,
                                         ast::kInvalidDecl, memory::Span{});
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
            hir_let.name = syms().interner().intern(var_name);
            hir_let.type = var_type;
            hir_let.init = init;

            auto hir_id = addHirExpr(hir::HirExpr{hir_let}, init_type);
            if (current_fn_ && !current_fn_->blocks.empty()) {
                auto &cur = current_fn_->blocks[currentBlock()];
                cur.insts.push(hir_id);
            }
        }
        break;
    }
    case ast::StmtKind::Assign: {
        const auto &n = std::get<ast::AssignNode>(node);
        auto target = visitExpr(n.target);
        auto value  = visitExpr(n.value);
        (void)target;
        (void)value;
        break;
    }
    case ast::StmtKind::Return: {
        const auto &n = std::get<ast::RetNode>(node);
        hir::HirExprId val     = hir::kInvalidHirExpr;
        types::TypeId val_type = types::kVoidType;
        if (n.value != ast::kInvalidExpr) {
            val = visitExpr(n.value);
            if (val != hir::kInvalidHirExpr)
                val_type = exprType(val);
        }

        if (val_type != types::kErrorType && current_fn_ret_type_ != types::kErrorType) {
            if (unifier_.unify(val_type, current_fn_ret_type_, n.span)) {
                types::TypeId concrete = current_fn_ret_type_;
                auto kind              = ctx_.types().kindOf(current_fn_ret_type_);
                if (kind == types::TypeKind::Int) {
                    auto &type =
                        std::get<types::TypeInt>(ctx_.types().lookup(current_fn_ret_type_));
                    if (type.width == types::IntWidth::Literal)
                        concrete = val_type;
                }
                if (resolveLiteralType(hir_, val, ctx_.types(), concrete)) {
                    auto &expr  = hir_.getExpr(val);
                    auto &lit   = std::get<hir::HirLiteral>(expr);
                    auto &int_t = std::get<types::TypeInt>(ctx_.types().lookup(concrete));
                    if (auto *err = checkIntOverflow(lit.i, int_t.width))
                        ctx_.diags().report(Severity::Error, TypeMismatch, err, n.span);
                }
            }
        }

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
