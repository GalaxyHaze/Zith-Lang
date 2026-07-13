#include "codegen-emit.hpp"

#include "ast/ast-node-utils.hpp"
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
    switch (hir::exprKind(expr)) {
    case hir::HirExprKind::Literal:
        return emitLiteral(std::get<hir::HirLiteral>(expr));
    case hir::HirExprKind::Binary:
        return emitBinary(std::get<hir::HirBinary>(expr), mod);
    case hir::HirExprKind::Unary:
        return emitUnary(std::get<hir::HirUnary>(expr), mod);
    case hir::HirExprKind::Let:
        return emitLet(std::get<hir::HirLet>(expr), mod);
    case hir::HirExprKind::Var:
        return emitVar(std::get<hir::HirVar>(expr));
    case hir::HirExprKind::Call:
        return emitCall(std::get<hir::HirCall>(expr), mod);
    case hir::HirExprKind::Ret:
        return emitRet(std::get<hir::HirRet>(expr), mod);
    case hir::HirExprKind::Branch:
        return emitBranch(std::get<hir::HirBranch>(expr), mod);
    case hir::HirExprKind::Jump:
        return emitJump(std::get<hir::HirJump>(expr), mod);
    case hir::HirExprKind::Phi:
        return nullptr;
    }
    return nullptr;
}

llvm::Value *CodeGenEmit::emitBody(const hir::HirFunction &fn, const hir::HirModule &mod) {
    if (fn.blocks.empty())
        return nullptr;
    if (!blocks_ || blocks_->empty())
        return nullptr;

    llvm::Value *last = nullptr;
    for (size_t i = 0; i < fn.blocks.size(); i++) {
        auto &block  = fn.blocks[i];
        auto *llvmBB = (*blocks_)[i];
        // Move builder to this block if it's not already inserted
        // (avoid moving if the block already has a terminator)
        builder_.SetInsertPoint(llvmBB);

        for (auto inst_id : block.insts) {
            last = emitExpr(inst_id, mod);
        }
        if (block.terminator != hir::kInvalidHirExpr) {
            emitExpr(block.terminator, mod);
        }
    }
    return last;
}

void CodeGenEmit::registerParams(const hir::HirFunction &fn, llvm::Function *llvmFn) {
    if (fn.decl_id == ast::kInvalidDecl)
        return;
    auto &decl   = astBuilder_.getDecl(fn.decl_id);
    auto *fnDecl = ast::asFn(decl);
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
    case types::TypeKind::Int: {
        auto &int_t   = std::get<types::TypeInt>(types_.lookup(lit.type));
        unsigned bits = 64;
        switch (int_t.width) {
        case types::IntWidth::I8:
        case types::IntWidth::U8:
            bits = 8;
            break;
        case types::IntWidth::I16:
        case types::IntWidth::U16:
            bits = 16;
            break;
        case types::IntWidth::I32:
        case types::IntWidth::U32:
            bits = 32;
            break;
        case types::IntWidth::I64:
        case types::IntWidth::U64:
            bits = 64;
            break;
        case types::IntWidth::I128:
        case types::IntWidth::U128:
            bits = 128;
            break;
        case types::IntWidth::Literal:
            bits = 64;
            break;
        }
        bool is_signed = int_t.width >= types::IntWidth::I8 && int_t.width <= types::IntWidth::I128;
        return llvm::ConstantInt::get(builder_.getContext(), llvm::APInt(bits, lit.i, is_signed));
    }
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
    llvm::Function *fn = nullptr;

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

llvm::Value *CodeGenEmit::emitJump(const hir::HirJump &jump, const hir::HirModule &mod) {
    (void)mod;
    if (!blocks_ || jump.target >= blocks_->size())
        return nullptr;
    auto *target = (*blocks_)[jump.target];
    return builder_.CreateBr(target);
}

llvm::Value *CodeGenEmit::emitBranch(const hir::HirBranch &branch, const hir::HirModule &mod) {
    if (!blocks_ || branch.then_block >= blocks_->size() || branch.else_block >= blocks_->size())
        return nullptr;
    auto *condVal = emitExpr(branch.cond, mod);
    if (!condVal)
        return nullptr;
    auto *thenBB = (*blocks_)[branch.then_block];
    auto *elseBB = (*blocks_)[branch.else_block];
    return builder_.CreateCondBr(condVal, thenBB, elseBB);
}

} // namespace zith::codegen
