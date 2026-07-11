#include "codegen.hpp"
#include "diagnostics/error-codes.hpp"

#ifdef ZITH_HAS_LLVM
#include "diagnostics/error-codes.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>

namespace zith::codegen {

namespace {

llvm::Type *lowerType(llvm::LLVMContext &context, const types::TypeIntern &types,
                      types::TypeId id) {
    const auto &type = types.lookup(id);
    if (const auto *integer = std::get_if<types::TypeInt>(&type)) {
        switch (integer->width) {
        case types::IntWidth::I8:
        case types::IntWidth::U8:
            return llvm::Type::getInt8Ty(context);
        case types::IntWidth::I16:
        case types::IntWidth::U16:
            return llvm::Type::getInt16Ty(context);
        case types::IntWidth::I32:
        case types::IntWidth::U32:
            return llvm::Type::getInt32Ty(context);
        default:
            return llvm::Type::getInt64Ty(context);
        }
    }
    if (std::holds_alternative<types::TypeFloat>(type))
        return llvm::Type::getDoubleTy(context);
    if (std::holds_alternative<types::TypeVoid>(type))
        return llvm::Type::getVoidTy(context);
    if (std::holds_alternative<types::TypePtr>(type))
        return llvm::PointerType::get(context, 0);
    return llvm::Type::getInt8Ty(context);
}

} // namespace

bool CodeGenerator::lowerToLlvm(const hir::HirModule &hir, const types::TypeIntern &types,
                                 const memory::StringInterner &interner,
                                 std::string_view target_triple, bool print_ir,
                                 diagnostics::DiagnosticEngine &diags) const {
    llvm::LLVMContext context;
    llvm::Module module("zith", context);
    if (!target_triple.empty())
        module.setTargetTriple(llvm::Triple(llvm::StringRef(target_triple.data(), target_triple.size())));

    for (size_t i = 0; i < hir.getFnCount(); ++i) {
        const auto &function = hir.getFn(i);
        std::vector<llvm::Type *> params;
        params.reserve(function.params.size());
        for (auto param : function.params)
            params.push_back(lowerType(context, types, param));
        auto *signature = llvm::FunctionType::get(lowerType(context, types, function.return_type),
                                                   params, false);
        auto name = interner.lookup(function.name);
        llvm::Function::Create(signature, llvm::Function::ExternalLinkage,
                               llvm::StringRef(name.data(), name.size()), module);
    }

    if (print_ir)
        module.print(llvm::outs(), nullptr);
    return true;
}

} // namespace zith::codegen

#else

namespace zith::codegen {

bool CodeGenerator::lowerToLlvm(const hir::HirModule &, const types::TypeIntern &,
                                 const memory::StringInterner &, std::string_view, bool,
                                 diagnostics::DiagnosticEngine &diags) const {
    diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIR,
                 "LLVM code generation is unavailable in this build", {});
    return false;
}

} // namespace zith::codegen

#endif
