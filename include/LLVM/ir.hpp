#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>

namespace zith {
namespace LLVM {
using IRBuilder = ::llvm::IRBuilder<>;
using LLVMContext = ::llvm::LLVMContext;
using Module = ::llvm::Module;
}
}