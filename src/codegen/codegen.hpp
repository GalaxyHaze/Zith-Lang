#pragma once

#include "ast/ast-builder.hpp"
#include "codegen-emit.hpp"
#include "codegen-type.hpp"
#include "hir/hir-module.hpp"
#include "memory/string-interner.hpp"
#include "symbols/symbol-table.hpp"
#include "types/type-intern.hpp"

#include <memory>
#include <string_view>

namespace llvm {
class LLVMContext;
class Module;
class Function;
} // namespace llvm

namespace zith::codegen {

class CodeGen {
public:
    CodeGen(const memory::StringInterner &interner, const symbols::SymbolTable &syms,
            const types::TypeIntern &types, const ast::AstBuilder &astBuilder);
    ~CodeGen();

    void emit(const hir::HirModule &hirModule, std::string_view moduleName);

    bool emitObject(const std::string &outputPath);
    llvm::Module *getModule();
    std::string printIR();

private:
    void emitFunction(const hir::HirFunction &fn, const hir::HirModule &mod);
    void emitExternFn(const hir::HirFunction &fn);

    std::unique_ptr<llvm::LLVMContext> ctx_;
    std::unique_ptr<llvm::Module> module_;
    const memory::StringInterner &interner_;
    const symbols::SymbolTable &syms_;
    const types::TypeIntern &types_;
    const ast::AstBuilder &astBuilder_;
};

} // namespace zith::codegen
