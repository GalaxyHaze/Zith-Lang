#include "zir/zir/zir-emitter.hpp"

#include "types/type-kind.hpp"

#include <cstring>
#include <string>

namespace zith::zir {

ZirEmitter::ZirEmitter(const hir::HirModule &hir, ZirModule &out, memory::Arena &arena)
    : hir_(hir), out_(out), arena_(arena) {}

ZirIndex ZirEmitter::addConst(int64_t v) {
    out_.constants.push(v);
    return static_cast<ZirIndex>(out_.constants.size() - 1);
}

ZirIndex ZirEmitter::addConst(double v) {
    out_.constants.push(v);
    return static_cast<ZirIndex>(out_.constants.size() - 1);
}

ZirIndex ZirEmitter::addConst(std::string_view v) {
    out_.constants.push(std::string(v));
    return static_cast<ZirIndex>(out_.constants.size() - 1);
}

namespace {

void emitLiteral(const hir::HirLiteral &lit, ZirEmitter *emitter, memory::DynArray<ZirInst> &code) {
    auto idx = emitter->addConst(lit.i);
    code.emplace(ZirOp::Push, idx);
}

void emitBinary(const hir::HirBinary &bin, memory::DynArray<ZirInst> &code) {
    ZirOp op = ZirOp::AddI;
    switch (bin.op) {
    case hir::HirBinaryOp::Add:
        op = ZirOp::AddI;
        break;
    case hir::HirBinaryOp::Sub:
        op = ZirOp::SubI;
        break;
    case hir::HirBinaryOp::Mul:
        op = ZirOp::MulI;
        break;
    case hir::HirBinaryOp::Div:
        op = ZirOp::DivI;
        break;
    case hir::HirBinaryOp::Eq:
    case hir::HirBinaryOp::Ne:
    case hir::HirBinaryOp::Lt:
    case hir::HirBinaryOp::Le:
    case hir::HirBinaryOp::Gt:
    case hir::HirBinaryOp::Ge:
        op = ZirOp::CmpI;
        break;
    default:
        op = ZirOp::AddI;
        break;
    }
    code.emplace(op);
}

void emitLet(const hir::HirLet &let, memory::DynArray<ZirInst> &code) {
    uint8_t slot = static_cast<uint8_t>(std::hash<std::string_view>{}(let.name) & 0x3);
    code.emplace(ZirOp::Store, 0, slot);
}

void emitVar(const hir::HirVar &var, memory::DynArray<ZirInst> &code) {
    uint8_t slot = static_cast<uint8_t>(std::hash<std::string_view>{}(var.name) & 0x3);
    code.emplace(ZirOp::Load, 0, slot);
}

void emitCall(const hir::HirCall &call, const hir::HirModule &hir, ZirEmitter *emitter,
              memory::DynArray<ZirInst> &code) {
    std::string_view callee_name;
    if (call.callee != hir::kInvalidHirExpr) {
        const auto &callee_expr = hir.getExpr(call.callee);
        if (std::holds_alternative<hir::HirVar>(callee_expr)) {
            callee_name = std::get<hir::HirVar>(callee_expr).name;
        }
    }

    if (callee_name == "write") {
        code.emplace(ZirOp::Write, 0, static_cast<uint8_t>(call.args.size()));
    } else if (callee_name == "input") {
        code.emplace(ZirOp::Input, 0, 0);
    } else {
        code.emplace(ZirOp::Call, 0, static_cast<uint8_t>(call.args.size()));
    }
}

} // anonymous namespace

void ZirEmitter::emit() {
    for (size_t i = 0; i < hir_.getFnCount(); i++) {
        const auto &fn = hir_.getFn(i);
        ZirFn zf(arena_);
        zf.name        = fn.name;
        zf.param_count = static_cast<uint8_t>(fn.params.size());
        zf.local_count = 4;
        zf.ret_type    = fn.return_type;

        ZirBlock block(arena_);
        memory::DynArray<ZirInst> &code = block.code;

        for (size_t blk_idx = 0; blk_idx < fn.blocks.size(); blk_idx++) {
            const auto &blk = fn.blocks[blk_idx];
            for (auto expr_id : blk.insts) {
                const auto &expr = hir_.getExpr(expr_id);

                if (auto *lit = std::get_if<hir::HirLiteral>(&expr)) {
                    emitLiteral(*lit, this, code);
                } else if (auto *bin = std::get_if<hir::HirBinary>(&expr)) {
                    emitBinary(*bin, code);
                } else if (auto *let = std::get_if<hir::HirLet>(&expr)) {
                    emitLet(*let, code);
                } else if (auto *var = std::get_if<hir::HirVar>(&expr)) {
                    emitVar(*var, code);
                } else if (auto *call = std::get_if<hir::HirCall>(&expr)) {
                    emitCall(*call, hir_, this, code);
                } else if (auto *ret = std::get_if<hir::HirRet>(&expr)) {
                    code.emplace(ZirOp::Ret, 0,
                                 static_cast<uint8_t>(ret->value != hir::kInvalidHirExpr ? 1 : 0));
                }
            }
        }

        if (code.empty() || code.back().op != ZirOp::Ret) {
            code.emplace(ZirOp::Ret, 0, 0);
        }

        zf.blocks.push(std::move(block));
        out_.functions.push(std::move(zf));
    }
}

} // namespace zith::zir
