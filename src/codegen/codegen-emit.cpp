#include "codegen-emit.hpp"

#include "types/type-kind.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

namespace zith::codegen {

CodeGenEmit::CodeGenEmit(llvm::IRBuilderBase &builder, CodeGenType &typeGen,
                         const memory::StringInterner &interner, const symbols::SymbolTable &syms,
                         const types::TypeIntern &types, const ast::AstBuilder &astBuilder)
    : builder_(builder), typeGen_(typeGen), interner_(interner), syms_(syms), types_(types),
      astBuilder_(astBuilder) {}

llvm::Value *CodeGenEmit::emitExpr(hir::HirExprId id, const hir::HirModule &mod) {
    auto &expr = mod.getExpr(id);
    return std::visit(
        [this, &mod](const auto &node) -> llvm::Value * {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, hir::HirLiteral>)
                return emitLiteral(node);
            else if constexpr (std::is_same_v<T, hir::HirBinary>)
                return emitBinary(node, mod);
            else if constexpr (std::is_same_v<T, hir::HirUnary>)
                return emitUnary(node, mod);
            else if constexpr (std::is_same_v<T, hir::HirCall>)
                return emitCall(node, mod);
            else if constexpr (std::is_same_v<T, hir::HirRet>)
                return emitRet(node, mod);
            else if constexpr (std::is_same_v<T, hir::HirLet>)
                return emitLet(node, mod);
            else if constexpr (std::is_same_v<T, hir::HirVar>)
                return emitVar(node);
            else if constexpr (std::is_same_v<T, hir::HirBranch> ||
                               std::is_same_v<T, hir::HirJump> || std::is_same_v<T, hir::HirPhi>)
                return nullptr;
            else
                return nullptr;
        },
        expr);
}

llvm::Value *CodeGenEmit::emitBody(const hir::HirFunction &fn, const hir::HirModule &mod) {
    if (fn.blocks.empty())
        return nullptr;

    llvm::Value *last = nullptr;
    auto &entry       = fn.blocks[0];
    for (auto inst_id : entry.insts) {
        last = emitExpr(inst_id, mod);
    }
    if (entry.terminator != hir::kInvalidHirExpr) {
        emitExpr(entry.terminator, mod);
    }
    return last;
}

void CodeGenEmit::registerParams(const hir::HirFunction &fn, llvm::Function *llvmFn) {
    if (fn.decl_id == ast::kInvalidDecl)
        return;
    auto &decl   = astBuilder_.getDecl(fn.decl_id);
    auto *fnDecl = std::get_if<ast::FnDeclNode>(&decl);
    if (!fnDecl)
        return;

    auto *argIt = llvmFn->arg_begin();
    for (size_t i = 0; i < fnDecl->params.size() && argIt != llvmFn->arg_end(); i++, ++argIt) {
        auto paramName = fnDecl->params[i].name;
        argIt->setName(llvm::StringRef(paramName.data(), paramName.size()));
        namedValues_[paramName] = {argIt, argIt->getType(), false};
    }
}

llvm::Value *CodeGenEmit::emitLiteral(const hir::HirLiteral &lit) {
    auto kind = types_.kindOf(lit.type);
    switch (kind) {
    case types::TypeKind::Int:
        return llvm::ConstantInt::get(builder_.getContext(), llvm::APInt(64, lit.i, true));
    case types::TypeKind::Bool:
        return llvm::ConstantInt::get(builder_.getContext(), llvm::APInt(1, lit.b ? 1 : 0));
    case types::TypeKind::Float:
        return llvm::ConstantFP::get(builder_.getContext(), llvm::APFloat(lit.f));
    case types::TypeKind::Char:
        return llvm::ConstantInt::get(llvm::Type::getInt8Ty(builder_.getContext()), lit.i, true);
    case types::TypeKind::Ptr: {
        auto str_data = interner_.lookup(lit.str_val);
        auto *str     = llvm::ConstantDataArray::getString(
            builder_.getContext(), llvm::StringRef(str_data.data(), str_data.size()), true);
        auto *module = builder_.GetInsertBlock()->getParent()->getParent();
        auto *global = new llvm::GlobalVariable(*module, str->getType(), true,
                                                llvm::GlobalValue::PrivateLinkage, str, ".str");
        auto *zero   = llvm::ConstantInt::get(llvm::Type::getInt32Ty(builder_.getContext()), 0);
        llvm::Value *indices[] = {zero, zero};
        return llvm::ConstantExpr::getInBoundsGetElementPtr(str->getType(), global, indices);
    }
    default:
        return llvm::Constant::getNullValue(llvm::Type::getVoidTy(builder_.getContext()));
    }
}

llvm::Value *CodeGenEmit::emitBinary(const hir::HirBinary &bin, const hir::HirModule &mod) {
    auto *lhs = emitExpr(bin.lhs, mod);
    auto *rhs = emitExpr(bin.rhs, mod);
    if (!lhs || !rhs)
        return nullptr;

    bool isFloat = lhs->getType()->isFloatingPointTy();
    switch (bin.op) {
    case hir::HirBinaryOp::Add:
        return isFloat ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
    case hir::HirBinaryOp::Sub:
        return isFloat ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
    case hir::HirBinaryOp::Mul:
        return isFloat ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
    case hir::HirBinaryOp::Div:
        return isFloat ? builder_.CreateFDiv(lhs, rhs) : builder_.CreateSDiv(lhs, rhs);
    case hir::HirBinaryOp::Rem:
        return isFloat ? builder_.CreateFRem(lhs, rhs) : builder_.CreateSRem(lhs, rhs);
    case hir::HirBinaryOp::Eq:
        return isFloat ? builder_.CreateFCmpOEQ(lhs, rhs) : builder_.CreateICmpEQ(lhs, rhs);
    case hir::HirBinaryOp::Ne:
        return isFloat ? builder_.CreateFCmpONE(lhs, rhs) : builder_.CreateICmpNE(lhs, rhs);
    case hir::HirBinaryOp::Lt:
        return isFloat ? builder_.CreateFCmpOLT(lhs, rhs) : builder_.CreateICmpSLT(lhs, rhs);
    case hir::HirBinaryOp::Le:
        return isFloat ? builder_.CreateFCmpOLE(lhs, rhs) : builder_.CreateICmpSLE(lhs, rhs);
    case hir::HirBinaryOp::Gt:
        return isFloat ? builder_.CreateFCmpOGT(lhs, rhs) : builder_.CreateICmpSGT(lhs, rhs);
    case hir::HirBinaryOp::Ge:
        return isFloat ? builder_.CreateFCmpOGE(lhs, rhs) : builder_.CreateICmpSGE(lhs, rhs);
    case hir::HirBinaryOp::And:
        return builder_.CreateAnd(lhs, rhs);
    case hir::HirBinaryOp::Or:
        return builder_.CreateOr(lhs, rhs);
    case hir::HirBinaryOp::Xor:
        return builder_.CreateXor(lhs, rhs);
    }
    return nullptr;
}

llvm::Value *CodeGenEmit::emitUnary(const hir::HirUnary &un, const hir::HirModule &mod) {
    auto *operand = emitExpr(un.operand, mod);
    if (!operand)
        return nullptr;

    switch (un.op) {
    case hir::HirUnaryOp::Neg:
        return builder_.CreateNeg(operand);
    case hir::HirUnaryOp::Not:
        return builder_.CreateNot(operand);
    case hir::HirUnaryOp::BitNot:
        return builder_.CreateNot(operand);
    case hir::HirUnaryOp::Ref:
    case hir::HirUnaryOp::Deref:
        return operand;
    }
    return nullptr;
}

llvm::Value *CodeGenEmit::emitCall(const hir::HirCall &call, const hir::HirModule &mod) {
    auto &callee_expr = mod.getExpr(call.callee);
    auto *callee_fn   = std::get_if<hir::HirLiteral>(&callee_expr);

    llvm::Function *fn = nullptr;
    if (call.resolved_fn != symbols::kInvalidSym) {
        auto &sym = syms_.get(call.resolved_fn);
        auto name = interner_.lookup(sym.name);
    }

    // For extern calls, resolve by finding the function in the module
    llvm::SmallVector<llvm::Value *, 8> args;
    for (auto arg_id : call.args) {
        auto *v = emitExpr(arg_id, mod);
        if (!v)
            return nullptr;
        args.push_back(v);
    }

    // Find callee function from the module context
    // The callee is typically the first arg — resolve through IRBuilder's module
    auto *module = builder_.GetInsertBlock()->getParent()->getParent();

    // For now, resolve callee from resolved_fn symbol
    if (call.resolved_fn != symbols::kInvalidSym) {
        auto &sym = syms_.get(call.resolved_fn);
        auto name = interner_.lookup(sym.name);
        fn        = module->getFunction(llvm::StringRef(name.data(), name.size()));
    }

    if (!fn)
        return nullptr;

    return builder_.CreateCall(fn, args);
}

llvm::Value *CodeGenEmit::emitRet(const hir::HirRet &ret, const hir::HirModule &mod) {
    if (ret.value == hir::kInvalidHirExpr)
        return builder_.CreateRetVoid();
    auto *val = emitExpr(ret.value, mod);
    if (!val)
        return nullptr;
    return builder_.CreateRet(val);
}

llvm::Value *CodeGenEmit::emitLet(const hir::HirLet &let, const hir::HirModule &mod) {
    auto *init = emitExpr(let.init, mod);
    if (!init)
        return nullptr;

    auto name      = interner_.lookup(let.name);
    auto *elemType = init->getType();
    auto *alloca   = builder_.CreateAlloca(elemType);
    builder_.CreateStore(init, alloca);
    namedValues_[name] = {alloca, elemType, true};
    return alloca;
}

llvm::Value *CodeGenEmit::emitVar(const hir::HirVar &var) {
    auto name = interner_.lookup(var.name);
    auto *nv  = namedValues_.get(name);
    if (nv) {
        if (nv->isAlloca)
            return builder_.CreateLoad(nv->elementType, nv->value);
        return nv->value;
    }
    return nullptr;
}

} // namespace zith::codegen
