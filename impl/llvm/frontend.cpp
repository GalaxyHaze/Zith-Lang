// impl/llvm/frontend.cpp — Zith IR → LLVM IR converter
#include "zith/vm.hpp"
#include "LLVM/LLVM.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <vector>

namespace zith::LLVM {

// ============================================================================
// Type Mapping: ZithTypeId → llvm::Type*
// ============================================================================

static llvm::Type* zithTypeToLLVM(ZithTypeId type_id,
                                   const std::vector<TypeEntry>& types,
                                   LLVMContext& ctx) {
    switch (type_id) {
        case ZITH_TYPE_VOID:   return llvm::Type::getVoidTy(ctx);
        case ZITH_TYPE_I1:     return llvm::Type::getInt1Ty(ctx);
        case ZITH_TYPE_I8:     return llvm::Type::getInt8Ty(ctx);
        case ZITH_TYPE_I16:    return llvm::Type::getInt16Ty(ctx);
        case ZITH_TYPE_I32:    return llvm::Type::getInt32Ty(ctx);
        case ZITH_TYPE_I64:    return llvm::Type::getInt64Ty(ctx);
        case ZITH_TYPE_I128:   return llvm::Type::getInt128Ty(ctx);
        case ZITH_TYPE_F16:    return llvm::Type::getHalfTy(ctx);
        case ZITH_TYPE_F32:    return llvm::Type::getFloatTy(ctx);
        case ZITH_TYPE_F64:    return llvm::Type::getDoubleTy(ctx);
        case ZITH_TYPE_PTR:    return llvm::Type::getInt8PtrTy(ctx);
        default:               return llvm::Type::getInt64Ty(ctx);
    }
}

// ============================================================================
// Frontend: converts a DecodedModule into an llvm::Module
// ============================================================================

class ZithFrontend {
    LLVMContext& ctx;
    std::unique_ptr<Module> llvm_module;
    const DecodedModule* zith_module;

    // ValueId → llvm::Value* mapping per function
    std::unordered_map<uint32_t, llvm::Value*> value_map;
    // BlockId → llvm::BasicBlock* mapping per function
    std::unordered_map<uint32_t, llvm::BasicBlock*> block_map;

public:
    ZithFrontend(LLVMContext& ctx, const DecodedModule* mod)
        : ctx(ctx), zith_module(mod) {}

    std::unique_ptr<Module> convert() {
        llvm_module = std::make_unique<Module>("zith_module", ctx);

        // Declare global variables
        for (auto& g : zith_module->globals) {
            declareGlobal(g);
        }

        // Declare and define functions
        for (auto& fn : zith_module->functions) {
            declareFunction(fn);
        }

        // Define function bodies
        for (size_t i = 0; i < zith_module->functions.size(); i++) {
            defineFunction(i);
        }

        return std::move(llvm_module);
    }

private:
    void declareGlobal(const GlobalEntry& g) {
        auto* ty = zithTypeToLLVM(
            static_cast<ZithTypeId>(g.type_id),
            zith_module->types, ctx
        );
        auto linkage = zithLinkageToLLVM(g.linkage);
        auto* gv = new llvm::GlobalVariable(
            *llvm_module, ty,
            g.flags & 1, // is_constant
            linkage,
            nullptr,
            zith_module->strings[g.name_string_id]
        );
        if (g.align_log2 > 0) {
            gv->setAlignment(llvm::MaybeAlign(1ULL << g.align_log2));
        }
    }

    llvm::GlobalValue::LinkageTypes zithLinkageToLLVM(ZithLinkage linkage) {
        switch (linkage) {
            case ZITH_LINKAGE_PRIVATE:              return llvm::GlobalValue::PrivateLinkage;
            case ZITH_LINKAGE_INTERNAL:             return llvm::GlobalValue::InternalLinkage;
            case ZITH_LINKAGE_EXTERNAL:             return llvm::GlobalValue::ExternalLinkage;
            case ZITH_LINKAGE_LINKONCE:             return llvm::GlobalValue::LinkOnceAnyLinkage;
            case ZITH_LINKAGE_WEAK:                 return llvm::GlobalValue::WeakAnyLinkage;
            case ZITH_LINKAGE_LINKONCE_ODR:         return llvm::GlobalValue::LinkOnceODRLinkage;
            case ZITH_LINKAGE_WEAK_ODR:             return llvm::GlobalValue::WeakODRLinkage;
            case ZITH_LINKAGE_AVAILABLE_EXTERNALLY: return llvm::GlobalValue::AvailableExternallyLinkage;
            case ZITH_LINKAGE_COMMON:               return llvm::GlobalValue::CommonLinkage;
            case ZITH_LINKAGE_APPENDING:            return llvm::GlobalValue::AppendingLinkage;
            case ZITH_LINKAGE_EXTERN_WEAK:          return llvm::GlobalValue::ExternalWeakLinkage;
            default:                                return llvm::GlobalValue::ExternalLinkage;
        }
    }

    void declareFunction(const FunctionEntry& fn) {
        // Function type from type table
        auto* fn_type = llvm::FunctionType::get(
            llvm::Type::getInt64Ty(ctx), // placeholder — needs type table lookup
            {}, false
        );

        auto* func = llvm::Function::Create(
            fn_type,
            zithLinkageToLLVM(fn.linkage),
            zith_module->strings[fn.name_string_id],
            *llvm_module
        );

        // Set attributes
        if (fn.attr_flags & ZITH_FN_ATTR_NOUNWIND) {
            func->addFnAttr(llvm::Attribute::NoUnwind);
        }
        if (fn.attr_flags & ZITH_FN_ATTR_READONLY) {
            func->addFnAttr(llvm::Attribute::ReadOnly);
        }
        if (fn.attr_flags & ZITH_FN_ATTR_READNONE) {
            func->addFnAttr(llvm::Attribute::ReadNone);
        }
        if (fn.attr_flags & ZITH_FN_ATTR_NORETURN) {
            func->addFnAttr(llvm::Attribute::NoReturn);
        }
    }

    void defineFunction(size_t fn_index) {
        // Full implementation would walk the function's bytecode,
        // create BasicBlocks, and emit LLVM instructions.
        // This is a skeleton — the full decoder walks ZBC instructions
        // and maps each to the corresponding LLVM IR instruction.
    }
};

std::unique_ptr<Module> ZithToLLVM(LLVMContext& ctx, const DecodedModule* mod) {
    ZithFrontend frontend(ctx, mod);
    return frontend.convert();
}

} // namespace zith::LLVM
