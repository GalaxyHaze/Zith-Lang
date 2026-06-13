#include "sema-pipeline.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-id.hpp"
#include "types/type-id.hpp"

#include <cstdlib>
#include <string>

namespace zith::sema {

namespace {

using ast::BinaryOp;
using ast::UnaryOp;
using ast::LitKind;
using diagnostics::Severity;
using diagnostics::err::TypeMismatch;
using diagnostics::err::UndefinedIdent;
using diagnostics::err::WrongArity;

zir::hir::HirBinaryOp mapBinaryOp(ast::BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:  return zir::hir::HirBinaryOp::Add;
    case BinaryOp::Sub:  return zir::hir::HirBinaryOp::Sub;
    case BinaryOp::Mul:  return zir::hir::HirBinaryOp::Mul;
    case BinaryOp::Div:  return zir::hir::HirBinaryOp::Div;
    case BinaryOp::Rest: return zir::hir::HirBinaryOp::Rem;
    case BinaryOp::Eq:   return zir::hir::HirBinaryOp::Eq;
    case BinaryOp::Ne:   return zir::hir::HirBinaryOp::Ne;
    case BinaryOp::Lt:   return zir::hir::HirBinaryOp::Lt;
    case BinaryOp::Le:   return zir::hir::HirBinaryOp::Le;
    case BinaryOp::Gt:   return zir::hir::HirBinaryOp::Gt;
    case BinaryOp::Ge:   return zir::hir::HirBinaryOp::Ge;
    case BinaryOp::And:  return zir::hir::HirBinaryOp::And;
    case BinaryOp::Or:   return zir::hir::HirBinaryOp::Or;
    case BinaryOp::Xor:  return zir::hir::HirBinaryOp::Xor;
    default:             return zir::hir::HirBinaryOp::Add;
    }
}

zir::hir::HirUnaryOp mapUnaryOp(ast::UnaryOp op) {
    switch (op) {
    case UnaryOp::Neg:   return zir::hir::HirUnaryOp::Neg;
    case UnaryOp::Not:   return zir::hir::HirUnaryOp::Not;
    default:             return zir::hir::HirUnaryOp::Neg;
    }
}

types::TypeId defaultTypeForLit(ast::LitKind kind, types::TypeIntern &types) {
    switch (kind) {
    case LitKind::Int:    return types.internInt(types::IntWidth::I64);
    case LitKind::Float:  return types.internFloat(types::FloatWidth::F64);
    case LitKind::Bool:   return types::kBoolType;
    case LitKind::String: return types::kErrorType; // string type TBD
    case LitKind::Char:   return types::kCharType;
    case LitKind::Nil:    return types::kVoidType;
    }
    return types::kErrorType;
}

} // anonymous namespace

SemaPipeline::SemaPipeline(import::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &hir_arena,
                           const memory::DynArray<import::SymId> *resolved)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_arena_(hir_arena),
      hir_(hir_arena), resolved_(resolved) {}

bool SemaPipeline::run(const ast::ProgramNode &program) {
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
            current_fn_ = &hir_.getFn(fn_idx);
            fn_idx++;

            // Set up parameter types (default to i64 for now)
            for (size_t i = 0; i < fn->params.size(); i++) {
                current_fn_->params.push(types::kErrorType);
            }
            current_fn_->return_type = types::kVoidType;
            current_fn_ret_type_ = types::kVoidType;

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
    return std::visit([this, id](auto &n) -> zir::hir::HirExprId {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, ast::LitValue>)    return visitLiteral(n);
        if constexpr (std::is_same_v<T, ast::IdentNode>)   return visitIdent(n, id);
        if constexpr (std::is_same_v<T, ast::BinaryNode>)  return visitBinary(n);
        if constexpr (std::is_same_v<T, ast::UnaryNode>)   return visitUnary(n);
        if constexpr (std::is_same_v<T, ast::CallNode>)    return visitCall(n);
        if constexpr (std::is_same_v<T, ast::BlockNode>)   return visitBlock(n);
        if constexpr (std::is_same_v<T, ast::IfNode>)      return visitIf(n);
        if constexpr (std::is_same_v<T, ast::WhileNode>)   return visitWhile(n);
        if constexpr (std::is_same_v<T, ast::FieldNode>)   return zir::hir::kInvalidHirExpr;
        if constexpr (std::is_same_v<T, ast::IndexNode>)   return zir::hir::kInvalidHirExpr;
        if constexpr (std::is_same_v<T, ast::RangeNode>)   return zir::hir::kInvalidHirExpr;
        if constexpr (std::is_same_v<T, ast::UnbodyNode>)  return zir::hir::kInvalidHirExpr;
        return zir::hir::kInvalidHirExpr;
    }, node);
}

zir::hir::HirExprId SemaPipeline::visitLiteral(const ast::LitValue &n) {
    zir::hir::HirLiteral lit{};
    lit.type = defaultTypeForLit(n.kind, ctx_.types());
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
    return hir_.addExpr(zir::hir::HirExpr{lit});
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
                                std::string("undefined identifier '") + std::string(n.name) + "'", {});
        return zir::hir::kInvalidHirExpr;
    }

    zir::hir::HirVar var;
    var.name = n.name;
    var.version = 0;
    return hir_.addExpr(zir::hir::HirExpr{var});
}

zir::hir::HirExprId SemaPipeline::visitBinary(const ast::BinaryNode &n) {
    auto lhs = visitExpr(n.lhs);
    auto rhs = visitExpr(n.rhs);
    if (lhs == zir::hir::kInvalidHirExpr || rhs == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    zir::hir::HirBinary bin;
    bin.lhs = lhs;
    bin.rhs = rhs;
    bin.op = mapBinaryOp(n.op);
    return hir_.addExpr(zir::hir::HirExpr{bin});
}

zir::hir::HirExprId SemaPipeline::visitUnary(const ast::UnaryNode &n) {
    auto operand = visitExpr(n.operand);
    if (operand == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    zir::hir::HirUnary un;
    un.op = mapUnaryOp(n.op);
    un.operand = operand;
    return hir_.addExpr(zir::hir::HirExpr{un});
}

zir::hir::HirExprId SemaPipeline::visitCall(const ast::CallNode &n) {
    auto callee = visitExpr(n.callee);
    if (callee == zir::hir::kInvalidHirExpr)
        return zir::hir::kInvalidHirExpr;

    memory::DynArray<zir::hir::HirExprId> hir_args{hir_arena_};
    for (auto arg : n.args) {
        auto hir_arg = visitExpr(arg);
        if (hir_arg != zir::hir::kInvalidHirExpr)
            hir_args.push(hir_arg);
    }

    zir::hir::HirCall call{callee, std::move(hir_args)};
    return hir_.addExpr(zir::hir::HirExpr{std::move(call)});
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
    // Return the trailing expression (block value)
    return last;
}

zir::hir::HirExprId SemaPipeline::visitIf(const ast::IfNode &n) {
    auto cond = visitExpr(n.cond);
    auto then_expr = visitExpr(n.then_branch);
    zir::hir::HirExprId else_expr = zir::hir::kInvalidHirExpr;
    if (n.else_branch != ast::kInvalidExpr)
        else_expr = visitExpr(n.else_branch);

    zir::hir::HirBranch branch;
    branch.cond = cond;
    // Both branches point to block 0 for now (skeleton)
    // Real block management will create proper then/else blocks
    branch.then_block = 0;
    branch.else_block = else_expr != zir::hir::kInvalidHirExpr ? 0 : 0;
    return hir_.addExpr(zir::hir::HirExpr{std::move(branch)});
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
            zir::hir::HirExprId init = zir::hir::kInvalidHirExpr;
            if (n.init != ast::kInvalidExpr) {
                init = pipeline.visitExpr(n.init);
            }

            pipeline.syms().declare(n.name, import::SymbolVisibility::Private, 0,
                                    import::SymKind::Variable, ast::kInvalidDecl,
                                    memory::Span{});

            zir::hir::HirLet hir_let;
            hir_let.name = n.name;
            hir_let.type = types::kErrorType;
            hir_let.init = init;

            auto hir_id = pipeline.hir_.addExpr(zir::hir::HirExpr{hir_let});
            if (pipeline.current_fn_) {
                // If we have a current function, add to its entry block
                if (!pipeline.current_fn_->blocks.empty())
                    pipeline.current_fn_->blocks[0].insts.push(hir_id);
            }
        }

        void operator()(const ast::AssignNode &n) {
            auto target = pipeline.visitExpr(n.target);
            auto value = pipeline.visitExpr(n.value);
            (void)target;
            (void)value;
        }

        void operator()(const ast::RetNode &n) {
            zir::hir::HirExprId val = zir::hir::kInvalidHirExpr;
            if (n.value != ast::kInvalidExpr) {
                val = pipeline.visitExpr(n.value);
            }

            zir::hir::HirRet ret;
            ret.value = val;
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
    (void)id;
    return types::kErrorType;
}

} // namespace zith::sema
