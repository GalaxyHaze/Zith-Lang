#include "codegen.hpp"

#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

namespace zith::codegen {

CodeGen::CodeGen(const memory::StringInterner &interner, const symbols::SymbolTable &syms,
                 const types::TypeIntern &types, const ast::AstBuilder &astBuilder)
    : ctx_(std::make_unique<llvm::LLVMContext>()), interner_(interner), syms_(syms), types_(types),
      astBuilder_(astBuilder) {}

CodeGen::~CodeGen() = default;

void CodeGen::emit(const hir::HirModule &hirModule, std::string_view moduleName) {
    module_ = std::make_unique<llvm::Module>(
        llvm::StringRef(moduleName.data(), moduleName.size()), *ctx_);

    for (size_t i = 0; i < hirModule.getFnCount(); i++) {
        auto &fn = hirModule.getFn(i);
        auto name = interner_.lookup(fn.name);

        bool is_extern = fn.blocks.empty();

        if (is_extern) {
            emitExternFn(fn);
        } else {
            emitFunction(fn, hirModule);
        }
    }
}

void CodeGen::emitExternFn(const hir::HirFunction &fn) {
    auto name = interner_.lookup(fn.name);

    CodeGenType typeGen(*ctx_, types_);
    llvm::SmallVector<llvm::Type *, 8> paramTypes;
    for (auto param_type : fn.params) {
        paramTypes.push_back(typeGen.lower(param_type));
    }
    auto *retType = typeGen.lower(fn.return_type);

    auto *fnType = llvm::FunctionType::get(retType, paramTypes, false);
    auto *llvmFn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                          llvm::StringRef(name.data(), name.size()), module_.get());
    llvmFn->setDoesNotThrow();
}

void CodeGen::emitFunction(const hir::HirFunction &fn, const hir::HirModule &mod) {
    auto name = interner_.lookup(fn.name);

    CodeGenType typeGen(*ctx_, types_);
    llvm::SmallVector<llvm::Type *, 8> paramTypes;
    for (auto param_type : fn.params) {
        paramTypes.push_back(typeGen.lower(param_type));
    }
    auto *retType = typeGen.lower(fn.return_type);

    auto *fnType = llvm::FunctionType::get(retType, paramTypes, false);
    auto *llvmFn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                          llvm::StringRef(name.data(), name.size()), module_.get());

    if (fn.blocks.empty())
        return;

    auto *entry = llvm::BasicBlock::Create(*ctx_, "entry", llvmFn);
    llvm::IRBuilder<> builder(entry);

    CodeGenEmit emit(builder, typeGen, interner_, syms_, types_, astBuilder_);
    emit.registerParams(fn, llvmFn);
    emit.emitBody(fn, mod);

    if (!builder.GetInsertBlock()->getTerminator()) {
        if (retType->isVoidTy())
            builder.CreateRetVoid();
    }

    llvm::verifyFunction(*llvmFn);
}

llvm::Module *CodeGen::getModule() {
    return module_.get();
}

std::string CodeGen::printIR() {
    std::string ir;
    llvm::raw_string_ostream os(ir);
    module_->print(os, nullptr);
    return ir;
}

bool CodeGen::emitObject(const std::string &outputPath) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string targetTriple = llvm::sys::getDefaultTargetTriple();
    module_->setTargetTriple(llvm::Triple(targetTriple));

    std::string error;
    auto triple = llvm::Triple(targetTriple);
    auto *target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        llvm::errs() << "Target lookup failed: " << error << "\n";
        return false;
    }

    llvm::TargetOptions options;
    std::unique_ptr<llvm::TargetMachine> targetMachine(target->createTargetMachine(
        targetTriple, "generic", "", options, llvm::Reloc::PIC_));
    if (!targetMachine) {
        llvm::errs() << "Failed to create TargetMachine\n";
        return false;
    }

    module_->setDataLayout(targetMachine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        llvm::errs() << "Failed to open output: " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager passManager;
    if (targetMachine->addPassesToEmitFile(passManager, dest, nullptr,
                                           llvm::CodeGenFileType::ObjectFile)) {
        llvm::errs() << "TargetMachine can't emit object file\n";
        return false;
    }

    passManager.run(*module_);
    dest.flush();

    return true;
}

} // namespace zith::codegen
