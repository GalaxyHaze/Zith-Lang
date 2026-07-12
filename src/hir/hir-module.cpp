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
                std::visit(
                    [&](auto &e) {
                        using T = std::decay_t<decltype(e)>;
                        if constexpr (std::is_same_v<T, HirLiteral>) {
                            if (e.type == static_cast<HirTypeId>(-1))
                                std::fprintf(out, "literal<?>");
                            else
                                std::fprintf(out, "literal %%t%u", e.type);
                        } else if constexpr (std::is_same_v<T, HirBinary>) {
                            std::fprintf(out, "binary %%e%u op %%e%u", e.lhs, e.rhs);
                        } else if constexpr (std::is_same_v<T, HirUnary>) {
                            std::fprintf(out, "unary op %%e%u", e.operand);
                        } else if constexpr (std::is_same_v<T, HirLet>) {
                            auto n = interner.lookup(e.name);
                            std::fprintf(out, "let %.*s", (int)n.size(), n.data());
                        } else if constexpr (std::is_same_v<T, HirVar>) {
                            auto n = interner.lookup(e.name);
                            std::fprintf(out, "var %.*s#%u", (int)n.size(), n.data(), e.version);
                        } else if constexpr (std::is_same_v<T, HirCall>) {
                            auto &callee = exprs_[e.callee];
                            if (auto *var = std::get_if<HirVar>(&callee)) {
                                auto n = interner.lookup(var->name);
                                std::fprintf(out, "call %.*s(", (int)n.size(), n.data());
                            } else {
                                std::fprintf(out, "call %%e%u(", e.callee);
                            }
                            for (size_t ai = 0; ai < e.args.size(); ++ai) {
                                if (ai > 0)
                                    std::fprintf(out, ", ");
                                std::fprintf(out, "%%e%u", e.args[ai]);
                            }
                            std::fprintf(out, ")");
                        } else if constexpr (std::is_same_v<T, HirRet>) {
                            if (e.value == kInvalidHirExpr)
                                std::fprintf(out, "ret void");
                            else
                                std::fprintf(out, "ret %%e%u", e.value);
                        } else {
                            std::fprintf(out, "<expr>");
                        }
                    },
                    expr);
                std::fprintf(out, "\n");
            }
            if (block.terminator != kInvalidHirExpr) {
                auto &term = exprs_[block.terminator];
                if (auto *ret = std::get_if<HirRet>(&term)) {
                    if (ret->value == kInvalidHirExpr)
                        std::fprintf(out, "  ret void\n");
                    else
                        std::fprintf(out, "  ret %%e%u\n", ret->value);
                } else {
                    std::fprintf(out, "  terminal %%e%u\n", block.terminator);
                }
            }
        }
        std::fprintf(out, "}\n\n");
    }
}

} // namespace zith::hir
