#include "sema-pipeline.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-id.hpp"
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

zir::hir::HirBinaryOp mapBinaryOp(ast::BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:
        return zir::hir::HirBinaryOp::Add;
    case BinaryOp::Sub:
        return zir::hir::HirBinaryOp::Sub;
    case BinaryOp::Mul:
        return zir::hir::HirBinaryOp::Mul;
    case BinaryOp::Div:
        return zir::hir::HirBinaryOp::Div;
    case BinaryOp::Rest:
        return zir::hir::HirBinaryOp::Rem;
    case BinaryOp::Eq:
        return zir::hir::HirBinaryOp::Eq;
    case BinaryOp::Ne:
        return zir::hir::HirBinaryOp::Ne;
    case BinaryOp::Lt:
        return zir::hir::HirBinaryOp::Lt;
    case BinaryOp::Le:
        return zir::hir::HirBinaryOp::Le;
    case BinaryOp::Gt:
        return zir::hir::HirBinaryOp::Gt;
    case BinaryOp::Ge:
        return zir::hir::HirBinaryOp::Ge;
    case BinaryOp::And:
        return zir::hir::HirBinaryOp::And;
    case BinaryOp::Or:
        return zir::hir::HirBinaryOp::Or;
    case BinaryOp::Xor:
        return zir::hir::HirBinaryOp::Xor;
    default:
        return zir::hir::HirBinaryOp::Add;
    }
}

zir::hir::HirUnaryOp mapUnaryOp(ast::UnaryOp op) {
    switch (op) {
    case UnaryOp::Neg:
        return zir::hir::HirUnaryOp::Neg;
    case UnaryOp::Not:
        return zir::hir::HirUnaryOp::Not;
    case UnaryOp::Ref:
        return zir::hir::HirUnaryOp::Ref;
    case UnaryOp::Deref:
        return zir::hir::HirUnaryOp::Deref;
    }
    return zir::hir::HirUnaryOp::Neg;
}

types::TypeId defaultTypeForLit(ast::LitKind kind, types::TypeIntern &types) {
    switch (kind) {
    case LitKind::Int:
        return types.internInt(types::IntWidth::I64);
    case LitKind::Float:
        return types.internFloat(types::FloatWidth::F64);
    case LitKind::Bool:
        return types::kBoolType;
    case LitKind::String:
        return types::kErrorType; // string type TBD
    case LitKind::Char:
        return types::kCharType;
    case LitKind::Nil:
        return types::kVoidType;
    }
    return types::kErrorType;
}

} // anonymous namespace

SemaPipeline::SemaPipeline(import::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &hir_arena,
                           const memory::DynArray<import::SymId> *resolved)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_arena_(hir_arena),
      hir_(hir_arena), resolved_(resolved), hir_types_(hir_arena) {}

zir::hir::HirExprId SemaPipeline::addHirExpr(zir::hir::HirExpr expr, types::TypeId type) {
    auto id = hir_.addExpr(std::move(expr));
    while (id >= hir_types_.size())
        hir_types_.push(types::kErrorType);
    hir_types_[id] = type;
    return id;
}

types::TypeId SemaPipeline::exprType(zir::hir::HirExprId id) const {
    if (id == zir::hir::kInvalidHirExpr || id >= hir_types_.size())
        return types::kErrorType;
    return hir_types_[id];
}

bool SemaPipeline::run(const ast::ProgramNode &program) {
    // Pass 0: register user-defined types (structs, enums, unions)
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *sd = std::get_if<ast::StructDeclNode>(&decl))
            ctx_.types().registerNamedType(sd->name, types::TypeKind::Struct);
        else if (auto *ed = std::get_if<ast::EnumDeclNode>(&decl))
            ctx_.types().registerNamedType(ed->name, types::TypeKind::Enum);
        else if (auto *ud = std::get_if<ast::UnionDeclNode>(&decl))
            ctx_.types().registerNamedType(ud->name, types::TypeKind::Union);
    }

    // First pass: register all fn declarations as HIR function stubs
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *fn = std::get_if<ast::FnDeclNode>(&decl)) {
            hir_.addFn(fn->name);
        }
    }

    // Second pass: visit each declaration body
    size_t fn_idx = 0;
    for (auto decl_id : program.decls) {
        auto &decl = ctx_.builder().getDecl(decl_id);
        if (auto *fn = std::get_if<ast::FnDeclNode>(&decl)) {
            current_fn_          = &hir_.getFn(fn_idx);
            current_fn_->decl_id = decl_id;
            fn_idx++;

            // Lower param types
            types::TypeLower lower(ctx_.builder(), ctx_.types(), ctx_.diags(), ctx_.syms());
            for (size_t i = 0; i < fn->params.size(); i++) {
                auto lowered = lower.lower(fn->params[i].type);
                current_fn_->params.push(lowered);
            }
            // Lower return type
            current_fn_->return_type = lower.lower(fn->return_type);
            current_fn_ret_type_     = current_fn_->return_type;

            if (fn->body != ast::kInvalidExpr) {
                // Create entry basic block
                auto &entry_block = current_fn_->blocks.emplace(hir_arena_);
                entry_block.insts = memory::DynArray<zir::hir::HirExprId>(hir_arena_);

                // Visit the body expression
                auto body_hir = visitExpr(fn->body);
                if (body_hir != zir::hir::kInvalidHirExpr) {
                    entry_block.insts.push(body_hir);
                }
            }
        }
    }

    ctx_.diags().emit();
    return !ctx_.diags().hasErrors();
}

zir::hir::HirExprId SemaPipeline::visitExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return zir::hir::kInvalidHirExpr;

    auto &node = ctx_.builder().getExpr(id);
    return std::visit(
        [this, id](auto &n) -> zir::hir::HirExprId {
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
                return zir::hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::IndexNode>)
                return zir::hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::RangeNode>)
                return zir::hir::kInvalidHirExpr;
            if constexpr (std::is_same_v<T, ast::UnbodyNode>)
                return zir::hir::kInvalidHirExpr;
            return zir::hir::kInvalidHirExpr;
        },
        node);
}

zir::hir::HirExprId SemaPipeline::visitLiteral(const ast::LitValue &n) {
    auto default_type = defaultTypeForLit(n.kind, ctx_.types());
    zir::hir::HirLiteral lit{};
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
    default:
        break;
    }
    return addHirExpr(zir::hir::HirExpr{lit}, default_type);
}

zir::hir::HirExprId SemaPipeline::visitIdent(const ast::IdentNode &n, ast::ExprId id) {
    import::SymId sym = import::kInvalidSym;
    if (resolved_ && id != ast::kInvalidExpr && id < resolved_->size())
        sym = (*resolved_)[id];
    if (sym == import::kInvalidSym)
        sym = ctx_.syms().lookup(n.name);
    if (sym == import::kInvalidSym) {
        if (!resolved_ || id >= resolved_->size() || (*resolved_)[id] != import::kInvalidSym)
            ctx_.diags().report(Severity::Error, UndefinedIdent,
                                std::string("undefined identifier '") + std::string(n.name) + "'",
                                n.span);
        return zir::hir::kInvalidHirExpr;
    }

    auto &data = ctx_.syms().get(sym);

    zir::hir::HirVar var;
    var.name    = n.name;
    var.version = 0;

    // Function names get unknown type so visitCall can do overload resolution
    types::TypeId ident_type = types::kErrorType;
    if (data.kind == import::SymKind::Fn)
        ident_type = ctx_.types().internUnknown();

    return addHirExpr(zir::hir::HirExpr{var}, ident_type);
}

zir::hir::HirExprId SemaPipeline::visitBinary(const ast::BinaryNode &n) {
    auto lhs = visitExpr(n.lhs);
    auto rhs = visitExpr(n.rhs);
    if (lhs == zir::hir::kInvalidHirExpr || rhs == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    types::TypeId result_type = exprType(lhs);
    {
        types::TypeId rhs_type = exprType(rhs);
        if (result_type != types::kErrorType && rhs_type != types::kErrorType)
            unifier_.unify(result_type, rhs_type);
    }

    zir::hir::HirBinary bin;
    bin.lhs = lhs;
    bin.rhs = rhs;
    bin.op  = mapBinaryOp(n.op);
    return addHirExpr(zir::hir::HirExpr{bin}, result_type);
}

zir::hir::HirExprId SemaPipeline::visitUnary(const ast::UnaryNode &n) {
    auto operand = visitExpr(n.operand);
    if (operand == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    auto operand_type = exprType(operand);

    zir::hir::HirUnary un;
    un.op      = mapUnaryOp(n.op);
    un.operand = operand;
    return addHirExpr(zir::hir::HirExpr{un}, operand_type);
}

zir::hir::HirExprId SemaPipeline::visitCall(const ast::CallNode &n) {
    auto callee = visitExpr(n.callee);
    if (callee == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    auto callee_type = exprType(callee);

    memory::DynArray<zir::hir::HirExprId> hir_args{hir_arena_};
    memory::DynArray<types::TypeId> arg_types{hir_arena_};
    for (auto arg : n.args) {
        auto hir_arg = visitExpr(arg);
        if (hir_arg != zir::hir::kInvalidHirExpr) {
            hir_args.push(hir_arg);
            arg_types.push(exprType(hir_arg));
        }
    }

    import::SymId resolved_fn = import::kInvalidSym;
    types::TypeId result_type = types::kErrorType;
    size_t match_count        = 0;

    // Overload resolution: if callee is an identifier (fn name)
    const auto &callee_expr = hir_.getExpr(callee);
    if (auto *var = std::get_if<zir::hir::HirVar>(&callee_expr)) {
        auto callee_name          = var->name;
        auto candidates           = syms().lookupAll(callee_name, hir_arena_);
        size_t fn_candidate_count = 0;
        for (auto sym_id : candidates) {
            auto &data = syms().get(sym_id);
            if (data.kind != import::SymKind::Fn)
                continue;
            fn_candidate_count++;
            if (data.decl_id == ast::kInvalidDecl)
                continue;

            auto &decl    = ctx_.builder().getDecl(data.decl_id);
            auto *fn_decl = std::get_if<ast::FnDeclNode>(&decl);
            if (!fn_decl)
                continue;

            // Arity check
            if (fn_decl->params.size() != n.args.size())
                continue;

            // Find matching HirFunction and check param types
            for (size_t fi = 0; fi < hir_.getFnCount(); fi++) {
                auto &hfn = hir_.getFn(fi);
                if (hfn.decl_id != data.decl_id)
                    continue;

                bool match = true;
                for (size_t i = 0; i < hfn.params.size(); i++) {
                    if (hfn.params[i] == types::kErrorType)
                        continue;
                    if (i >= arg_types.size() || arg_types[i] == types::kErrorType)
                        continue;
                    if (!unifier_.isAssignable(hfn.params[i], arg_types[i])) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    match_count++;
                    if (match_count == 1) {
                        resolved_fn = sym_id;
                        result_type = hfn.return_type;
                    }
                }
                break; // found the HirFunction for this decl
            }
        }

        if (match_count == 0 && fn_candidate_count > 0) {
            if (n.args.size() > 0) {
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
            resolved_fn = import::kInvalidSym;
            result_type = types::kErrorType;
        }
    }

    // Fallback: check callee as function type (fn pointers, etc.)
    if (resolved_fn == import::kInvalidSym && callee_type != types::kErrorType) {
        auto fn_ptr = std::get_if<types::TypeFn>(&ctx_.types().lookup(callee_type));
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

    zir::hir::HirCall call{callee, std::move(hir_args)};
    call.resolved_fn = resolved_fn;
    return addHirExpr(zir::hir::HirExpr{std::move(call)}, result_type);
}

zir::hir::HirExprId SemaPipeline::visitBlock(const ast::BlockNode &n) {
    // Process statements, collecting the last expression as trailing
    zir::hir::HirExprId last = zir::hir::kInvalidHirExpr;
    for (auto stmt_id : n.stmts) {
        visitStmt(stmt_id);
    }
    if (n.trailing != ast::kInvalidExpr) {
        last = visitExpr(n.trailing);
    }
    // Return the trailing expression with its type
    return last;
}

zir::hir::HirExprId SemaPipeline::visitIf(const ast::IfNode &n) {
    auto cond                     = visitExpr(n.cond);
    auto then_expr                = visitExpr(n.then_branch);
    zir::hir::HirExprId else_expr = zir::hir::kInvalidHirExpr;
    if (n.else_branch != ast::kInvalidExpr)
        else_expr = visitExpr(n.else_branch);

    types::TypeId result_type = types::kVoidType;
    if (then_expr != zir::hir::kInvalidHirExpr) {
        result_type = exprType(then_expr);
        if (else_expr != zir::hir::kInvalidHirExpr) {
            auto else_type = exprType(else_expr);
            if (result_type != types::kErrorType && else_type != types::kErrorType)
                unifier_.unify(result_type, else_type);
        }
    }

    zir::hir::HirBranch branch;
    branch.cond = cond;
    // Both branches point to block 0 for now (skeleton)
    // Real block management will create proper then/else blocks
    branch.then_block = 0;
    branch.else_block = else_expr != zir::hir::kInvalidHirExpr ? 0 : 0;
    return addHirExpr(zir::hir::HirExpr{std::move(branch)}, result_type);
}

zir::hir::HirExprId SemaPipeline::visitWhile(const ast::WhileNode &n) {
    auto cond = visitExpr(n.cond);
    auto body = visitExpr(n.body);
    // While loops don't produce a useful value in HIR yet
    return body;
}

void SemaPipeline::visitStmt(ast::StmtId id) {
    if (id == ast::kInvalidStmt)
        return;

    auto &node = ctx_.builder().getStmt(id);

    struct StmtVisitor {
        SemaPipeline &pipeline;

        void operator()(const ast::LetNode &n) {
            auto sym_kind = import::SymKind::Variable;

            // Determine declared type from annotation or init
            types::TypeId decl_type = types::kErrorType;

            // Check type annotation if present
            if (n.type_annot != ast::kInvalidTypeExpr) {
                types::TypeLower lower(pipeline.ctx_.builder(), pipeline.ctx_.types(),
                                       pipeline.ctx_.diags(), pipeline.ctx_.syms());
                decl_type = lower.lower(n.type_annot);
            }

            zir::hir::HirExprId init = zir::hir::kInvalidHirExpr;
            types::TypeId init_type  = types::kErrorType;
            if (n.init != ast::kInvalidExpr) {
                init = pipeline.visitExpr(n.init);
                if (init != zir::hir::kInvalidHirExpr) {
                    init_type = pipeline.exprType(init);
                }
            }

            // If we have both annotation and init, unify them
            if (n.type_annot != ast::kInvalidTypeExpr && init != zir::hir::kInvalidHirExpr) {
                if (decl_type != types::kErrorType && init_type != types::kErrorType)
                    pipeline.unifier_.unify(decl_type, init_type);
            }

            for (auto var_name : n.names) {
                pipeline.syms().declare(var_name, import::SymbolVisibility::Private, 0, sym_kind,
                                        ast::kInvalidDecl, memory::Span{});

                zir::hir::HirLet hir_let;
                hir_let.name = var_name;
                hir_let.type = init_type;
                hir_let.init = init;

                auto hir_id = pipeline.addHirExpr(zir::hir::HirExpr{hir_let}, init_type);
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
            zir::hir::HirExprId val = zir::hir::kInvalidHirExpr;
            types::TypeId val_type  = types::kVoidType;
            if (n.value != ast::kInvalidExpr) {
                val = pipeline.visitExpr(n.value);
                if (val != zir::hir::kInvalidHirExpr)
                    val_type = pipeline.exprType(val);
            }

            // Check return type compatibility (skip if either is error sentinel)
            if (val_type != types::kErrorType && pipeline.current_fn_ret_type_ != types::kErrorType)
                pipeline.unifier_.unify(val_type, pipeline.current_fn_ret_type_);

            zir::hir::HirRet ret;
            ret.value   = val;
            auto hir_id = pipeline.hir_.addExpr(zir::hir::HirExpr{ret});
            if (pipeline.current_fn_ && !pipeline.current_fn_->blocks.empty())
                pipeline.current_fn_->blocks[0].insts.push(hir_id);
        }

        void operator()(ast::ExprId expr_id) {
            auto result = pipeline.visitExpr(expr_id);
            if (result != zir::hir::kInvalidHirExpr && pipeline.current_fn_ &&
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
    if (hir_id == zir::hir::kInvalidHirExpr)
        return types::kErrorType;
    return exprType(hir_id);
}

} // namespace zith::sema
