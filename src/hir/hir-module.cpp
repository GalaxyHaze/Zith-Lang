#include "hir-module.hpp"

#include <cstdio>

namespace zith::hir {

HirModule::HirModule(memory::Arena &arena) : exprs_(arena), fns_(arena) {}

HirExprId HirModule::addExpr(HirExpr expr) {
    HirExprId id = static_cast<HirExprId>(exprs_.size());
    exprs_.push(std::move(expr));
    return id;
}

HirFunction &HirModule::addFn(memory::InternedId name) {
    fns_.emplace(exprs_.arena());
    fns_.back().name = name;
    return fns_.back();
}

HirFunction &HirModule::getFn(size_t idx) {
    return fns_[idx];
}
const HirExpr &HirModule::getExpr(HirExprId id) const {
    return exprs_[id];
}
HirExpr &HirModule::getExprMut(HirExprId id) {
    return exprs_[id];
}
const HirFunction &HirModule::getFn(size_t idx) const {
    return fns_[idx];
}

size_t HirModule::getFnCount() const {
    return fns_.size();
}

void HirModule::dump(FILE *out, const memory::StringInterner &interner) const {
    for (size_t fi = 0; fi < fns_.size(); ++fi) {
        auto &fn     = fns_[fi];
        auto fn_name = interner.lookup(fn.name);
        if (fn.blocks.empty()) {
            std::fprintf(out, "fn %.*s : extern\n", (int)fn_name.size(), fn_name.data());
            continue;
        }
        std::fprintf(out, "fn %.*s", (int)fn_name.size(), fn_name.data());
        std::fprintf(out, "(");
        for (size_t pi = 0; pi < fn.params.size(); ++pi) {
            if (pi > 0)
                std::fprintf(out, ", ");
            std::fprintf(out, "%%p%zu", pi);
        }
        std::fprintf(out, ")");
        std::fprintf(out, " -> %%t%u", fn.return_type);
        std::fprintf(out, " {\n");
        for (auto &block : fn.blocks) {
            for (auto inst_id : block.insts) {
                std::fprintf(out, "  %%e%u = ", inst_id);
                auto &expr = exprs_[inst_id];
                switch (exprKind(expr)) {
                case HirExprKind::Literal: {
                    auto &lit = std::get<HirLiteral>(expr);
                    if (lit.type == static_cast<HirTypeId>(-1))
                        std::fprintf(out, "literal<?>");
                    else
                        std::fprintf(out, "literal %%t%u", lit.type);
                    break;
                }
                case HirExprKind::Binary: {
                    auto &bin = std::get<HirBinary>(expr);
                    std::fprintf(out, "binary %%e%u op %%e%u", bin.lhs, bin.rhs);
                    break;
                }
                case HirExprKind::Unary: {
                    auto &un = std::get<HirUnary>(expr);
                    std::fprintf(out, "unary %%t%u op %%e%u", un.type, un.operand);
                    break;
                }
                case HirExprKind::Let: {
                    auto &let = std::get<HirLet>(expr);
                    auto n    = interner.lookup(let.name);
                    std::fprintf(out, "let %.*s", (int)n.size(), n.data());
                    break;
                }
                case HirExprKind::Var: {
                    auto &var = std::get<HirVar>(expr);
                    auto n    = interner.lookup(var.name);
                    std::fprintf(out, "var %.*s#%u", (int)n.size(), n.data(), var.version);
                    break;
                }
                case HirExprKind::Call: {
                    auto &call   = std::get<HirCall>(expr);
                    auto &callee = exprs_[call.callee];
                    if (exprKind(callee) == HirExprKind::Var) {
                        auto &var = std::get<HirVar>(callee);
                        auto n    = interner.lookup(var.name);
                        std::fprintf(out, "call %.*s(", (int)n.size(), n.data());
                    } else {
                        std::fprintf(out, "call %%e%u(", call.callee);
                    }
                    for (size_t ai = 0; ai < call.args.size(); ++ai) {
                        if (ai > 0)
                            std::fprintf(out, ", ");
                        std::fprintf(out, "%%e%u", call.args[ai]);
                    }
                    std::fprintf(out, ")");
                    break;
                }
                case HirExprKind::Ret: {
                    auto &ret = std::get<HirRet>(expr);
                    if (ret.value == kInvalidHirExpr)
                        std::fprintf(out, "ret void");
                    else
                        std::fprintf(out, "ret %%e%u", ret.value);
                    break;
                }
                case HirExprKind::Jump: {
                    auto &jump = std::get<HirJump>(expr);
                    std::fprintf(out, "jump bb%u", jump.target);
                    break;
                }
                case HirExprKind::Branch: {
                    auto &branch = std::get<HirBranch>(expr);
                    std::fprintf(out, "branch %%e%u -> bb%u : bb%u", branch.cond, branch.then_block,
                                 branch.else_block);
                    break;
                }
                case HirExprKind::Phi:
                    std::fprintf(out, "<expr>");
                    break;
                }
                std::fprintf(out, "\n");
            }
            if (block.terminator != kInvalidHirExpr) {
                auto &term = exprs_[block.terminator];
                switch (exprKind(term)) {
                case HirExprKind::Ret: {
                    auto &ret = std::get<HirRet>(term);
                    if (ret.value == kInvalidHirExpr)
                        std::fprintf(out, "  ret void\n");
                    else
                        std::fprintf(out, "  ret %%e%u\n", ret.value);
                    break;
                }
                case HirExprKind::Jump: {
                    auto &jump = std::get<HirJump>(term);
                    std::fprintf(out, "  jump bb%u\n", jump.target);
                    break;
                }
                case HirExprKind::Branch: {
                    auto &branch = std::get<HirBranch>(term);
                    std::fprintf(out, "  branch %%e%u -> bb%u : bb%u\n", branch.cond,
                                 branch.then_block, branch.else_block);
                    break;
                }
                default:
                    std::fprintf(out, "  terminal %%e%u\n", block.terminator);
                    break;
                }
            }
        }
        std::fprintf(out, "}\n\n");
    }
}

} // namespace zith::hir
